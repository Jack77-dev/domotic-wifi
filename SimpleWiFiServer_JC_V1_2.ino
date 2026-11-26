const char *version = "V1.7 du 22/10/2026";
/*
 WiFi Web Server 
 22/10/2026 : V1.7
              integration optimisation serial_management par IA Gemini (watchdog serial2, optimisation memoire via char[] vs String)
 08/08/2026 : V1.6
              ajout crontab each hour : logger date-time, bug crontab sur jour semaine
 13/07/2026 : V1.5
              ajout commande DISPLAY*message a afficher sur page web
 13/03/2026 : V1.4
              correction bug local time (ajout setenv et tzset())
              url du serveur forcee a 192.168.1.99
              ajout de la fonction CRONTAB
 31/01/2026 : V1.3
			        ajout UrlDecode pour decodage html de la ligne GET
			        ajout command_trt pour execution des commandes standards
 19/01/2025 : V1.2 
              integration serial network management
              integration du ntp pour time management
 08/11/2025 : V1.1
              creation page html formattee
              ajout du port serial2 pour communiquer avec le switch zigbee arduino
 23/10/2025 : V1 connecté sur livebox
              comporte 2 liens On/Off de la builtin LED et un formulaire de saisie 2 zones + bouton Envoi type GET

 A simple web server that lets you blink an LED via the web.
 This sketch will print the IP address of your WiFi Shield (once connected)
 to the Serial monitor. From there, you can open that address in a web browser
 to turn on and off the LED on pin 5.

 If the IP address of your shield is yourAddress:
 http://yourAddress/H turns the LED on
 http://yourAddress/L turns it off

 This example is written for a network using WPA2 encryption. For insecure
 WEP or WPA, change the Wifi.begin() call and use Wifi.setMinSecurity() accordingly.

 Circuit:
 * WiFi shield attached
 * LED attached to pin 5

 created for arduino 25 Nov 2012
 by Tom Igoe

ported for sparkfun esp32
31.01.2017 by Jan Hendrik Berlin

 */

#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"
#include "dictionary.h"
#include <vector>
#include "crontable.h"
////////////////////////////////////////////////////////////
//           SERIAL MANAGEMENT DECLARATIONS
bool xTrace = true;                   // false will stop logging
char MySerialName[] = "WIFI";  // device name (required) for serial management
#include "serial_management.h"
////////////////////////////////////////////////////////////

int ledPin = RGB_BUILTIN;
//int tx2pin = 21;
//int rx2pin = 22;

const char *ssid = "";
const char *password = "";

/* NTP server configuration */
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600*0;
const int daylightOffset_sec = 3600*0;
const String time_zone = "CET-1CEST,M3.5.0/2,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)
const long interval_time_update = 60000*15; // 15 minutes
/* Time structure */
long last_time = 0; long last_minute = 0; long last_hour = 0; long last_seconde = 0;
struct tm timeinfo; time_t secondes; time_t compute_secondes; long settime_secondes;
/* CRONTAB */
Dictionary crontab("crontab");        // sauvegarde crontable en NVS
String zcrontab = "* * * * * ;Commande";
std::vector<String> crontable;        // copie courante de crontab

static uint32_t nb_loop = 0; static uint32_t average_speed=0;
String inputString = "";      // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete
String InputCommand = "";     // a string to hold last incoming command
String ZoneTravail = "";
//
// code HTML Page Accueil
//
// <meta http-equiv="refresh" content="15"> ne fonctionne pas
const String page_accueil = R"***(
  <html lang="fr">
  <head>
  <meta charset="utf-8">
  <title>SERVEUR DOMOTIQUE</title>
  </head>
  <body>
	<div class="container">
		<div class="carre carre-bleu">
			<p>
				<a class="link" href="H">Allumer la LED</a>
			</p>
		</div>
		<div class="carre carre-gris">
			<p>
				<a class="link" href="L">Eteindre la LED</a>
			</p>
		</div>
	</div>
  <br>
  <form action="/submit-data" method="get">
    <br>
      <div class="form-group">
        <label for="command">Commande :</label>
        <input type="text" id="command" name="command"> 
        <button type="submit">Envoyer</button>
      </div>
      (syntaxe: Dest;commande)
      <!--  suppression champ parametre
      <div class="form-group">
        <label for="valeur">parametre :</label>
        <input type="text" id="valeur" name="valeur">
      </div>
      <br>
      -->
      
      
    </form>
 
	<style>
		.container{
			display:flex;
		}
    .link { color: white; }
		.carre{
			display:flex;
			color:white;
			width: 25%;
			height: 25px;
		}
		.carre-bleu{ background-color:blue;	}
		.carre-gris{ background-color:grey;	}
		.carre p{ margin: auto; }
    .console { background-color:black ; color:white; width: 100%; }
    .form-group { width: 400px;
            display: flex;
            align-items: center;
            margin-bottom: 15px;
        }
    label { width: 90px; }
    input[type="text"] { 
            flex-grow: 1;
            padding: 10px;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 16px;
        }
	</style>
  <hr>
  Messages systeme :<br>

    )***";
const String pied_page = R"***(
  </body>
  </html>
  )***";

NetworkServer server(80);
int Trace = 0; 
String currentLine = "";
String zone_message = ""; 

void page_html_1(NetworkClient client) {
  client.println(page_accueil);
  String ztexte="<div class=""console"">"; client.print(ztexte);
  client.println(zone_message);
  ztexte="</div>"; client.print(ztexte);
  client.println(pied_page);
  
  // The HTTP response ends with another blank line:
  client.println();
  }

String UrlDecode(String url){             // conversion des sequences HTML en caractere ascii
  char *percent_enc[34] = {"%20", "%22", "%25", "%2D", "%2E", "%3C", "%3E", "%5C", "%5E", "%5F", "%60", "%7B", "%7C", "%7D", "%7E", "%21", "%23", "%24", "%25", "%26", "%27", "%28", "%29", "%2A", "%2B", "%2C", "%2F", "%3A", "%3B", "%3D", "%3F", "%40", "%5B", "%5D"};
  char *url_decoded[34] = {" ", "\"", "%", "-", ".", "<", ">", "\\", "^", "_", "`", "{", "|", "}", "~", "!", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "/", ":", ";", "=", "?", "@", "[", "]"};
  char * pch;
  char str[255]; url.toCharArray(str, url.length()+1);

  //if (Trace=1) { Serial.print( "Before Decoding: "); Serial.println(str); }

  //iterate through the percent encoding array
  for (int i = 0; i < 34; ++i) {
    //find first percent url encoding in string
    pch = strstr (str, percent_enc[i]);
    while ( pch != NULL ) {
      //replace url encoding with corresponding character
      strncpy (pch, url_decoded[i], strlen(url_decoded[i]));

      //remove any remaining characters that were part of the url encoding
      memmove(pch + strlen(url_decoded[i]), pch + strlen(percent_enc[i]), strlen(pch + strlen(url_decoded[i])));

      //find next percent url encoding in string
      pch = strstr (str, percent_enc[i]);
    }
  }

  //if (Trace=1) { Serial.print( "After Decoding: "); Serial.println(str); }
  String zdecode=str;
  return(zdecode);
  }

void command_trt(String input_cmd){       // decodage de la ligne GET/XX ou execution du champ commande
  //
  if (!input_cmd.startsWith("GET ") ) { return; } // process only GET line
  //if ( input_cmd.indexOf("favicon.ico")) { return; }   // skip favicon line
  String command = UrlDecode(input_cmd);                // decodage caracteres speciaux sur url
  //  traitement url GET
  int i = command.lastIndexOf("HTTP/");                 // nettoyage ligne GET
  if ( i!=-1 ) { command = command.substring(0,i-1); }
  if (Trace==1) { Serial.print("command to process: "); Serial.println(command);}
  if (command.endsWith("/H")) {
    digitalWrite(ledPin, HIGH);                         // GET /H turns the LED on
    Serial.println("set Pin ON");
    }
  else if (command.endsWith("/L")) {
    digitalWrite(ledPin, LOW);                          // GET /L turns the LED off
    Serial.println("set Pin OFF");
    }
  else if (command.endsWith("/Trace")) {                 // GET /Trace Toggles mode Trace
    if (Trace==1) {Trace=0;} else {Trace=1;}
    Serial.print("Toggle Trace: ");Serial.println(Trace);
    }
  else if (command.indexOf("/submit-data?")) {          // sinon Decodage des champs du GET
    InputCommand = command.substring(command.indexOf("/submit-data?command=")+21); 
    if (InputCommand.startsWith(">>")) {                // envoie commande sur serial port
      String champ=";"+InputCommand.substring(2); Serial.println(champ);
      serial_write(champ);
      return;
    }
    ExecCommande(InputCommand);
    //string_split();
    }
  }

void string_split() {   // decodage des champs du GET (format : COMMANDE=xxxx&VALEUR=zzzzzz) et exec des commandes
  /* *
 * @brief 
  ecrire InputCommand sur serial2 (format "ZIGBEE;Commande*Param")
  ou exec par WIFI du param Valeur (format "WIFI;Commande*Param")
  
  if (Trace==1) { Serial.println(InputCommand); }
  if (InputCommand.startsWith("WIFI;")) {    // traitement commande pour interface wifi
    ExecCommande(InputCommand);
  }
  else if (InputCommand.startsWith("ZIGBEE;")) {     // traitement commande pour liason serie
    String champ=";"+InputCommand; Serial.println(champ);
    serial_write(champ);
  }
  //
  else { if (Trace==1) {Serial.print("Split: Invalid field syntax = "); Serial.println(InputCommand);  } }
  //Serial.println("sortie string_split");
  */
  }

void printLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");    
    return;
  }
  Serial.println(&timeinfo, "%A, %d %B %Y %H:%M:%S");
  }

void logLocalTime() {
  char buffer[80];
  sprintf(buffer, "Wifi: %02d/%02d/%d %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  logmessage(String(buffer));
  }

void logmessage(String zMessage) {        //ajout un message en tete de zone_message
  zone_message = zMessage+"<br>"+zone_message;  
  purgeZone_Message();
  }
void purgeZone_Message(){
  while (zone_message.length() > 2000) {                    // maintenir zone_message en dessous de 2000 caracteres
      int i=zone_message.lastIndexOf('<br>');
      if (i>0) { zone_message = zone_message.substring(0, i); }
    }
  }

int  ExecCommande(String commande) {         // DECODAGE ET EXECUTION DES COMMANDES STANDARDS format: "EMET;commande"
  Serial.println("ExecCommande: "+commande);
  
  // On convertit temporairement le String en char* pour travailler proprement
  const char* cmd_ptr = commande.c_str();
  
  // On cherche le premier point-virgule
  const char* sep_ptr = strchr(cmd_ptr, ';');
  String SRC=""; String action=""; int i;

  if (sep_ptr != nullptr) {
    // Calcul de la longueur de SRC (tout ce qui est avant le ';')
    int src_len = sep_ptr - cmd_ptr;
    
    // Extraction de SRC (soit via String, soit directement)
    SRC = commande.substring(0, src_len);
    
    // Extraction de l'action : tout ce qui est après le ';' jusqu'à la fin !
    action = String(sep_ptr + 1); 
    
    Serial.printf("SRC extrait : %s\n", SRC.c_str());
    Serial.print("Action extraite : "); Serial.println(action);
  } else {
    Serial.println("Erreur : Aucun séparateur ';' trouvé dans la commande.");
  }

  /* char sep=';'; String SRC=""; String action=""; int i;
  i=commande.indexOf(sep);
  if (i>0){
    SRC=commande.substring(0,i);
    Serial.printf("longueur: %d\n",commande.length());
    action=commande.substring(i+1, commande.length()-i);
  }
  */
  Serial.printf("Source: %s, Action: %s\n", SRC.c_str(), action.c_str());
  /////////// fonction du module WIFI //////////////////////////////////////
  if (action.startsWith("REBOOT*")) {          // reboot device
    Serial.println("Rebooting..."); MyDelay(1000);
    ESP.restart();
  }
  else if (action.startsWith("SETTIME*")) {          // set system time on Device
    secondes=action.substring(8).toDouble(); settime_secondes=millis()/1000;
    timeinfo=*localtime(&secondes); Serial.println(&timeinfo, "settime: %A, %d %B %Y %H:%M:%S");
  }
  else if (action.startsWith("DISPLAY*")) {          // display inf par ajout ligne texte dans zone_message
    zone_message = action.substring(8)+"<br>"+zone_message;   //ajout message en tete de zone
    purgeZone_Message();    // limiter longueur de zone_message
  }
  else if (action.startsWith("GETTIME*")) {          // ask system time
    if (getLocalTime(&timeinfo)) {
      String texte; texte.reserve(64); 
      char r[9]; dtostrf(time(&secondes),8,0,r);
      texte=';'; texte+=SRC;  texte+=";SETTIME*"; texte += r;
      Serial.println(texte);
      serial_write(texte);      // retourner heure au demandeur via uart serie
    }
  }
  else if (action.startsWith("DSPTIME*")) { logLocalTime(); }    // display time on monitor
  //
  else if (action.startsWith("CRON")) {                           // Action type Crontab
    crontab.begin();
    if (action.startsWith("CRONLIST*")) { listNvsKeys("crontab"); }
    else if (action.startsWith("CRONADD*")) {                     // Ajout/modification une Entree, format CRONADD*KEY/crontabEntry
      String crontab_entry = action.substring(8);
      i=crontab_entry.indexOf("/");
      if (i>0){
        String EntryKey=crontab_entry.substring(0,i);
        String Entry=crontab_entry.substring(i+1);
        crontab.putKey(EntryKey.c_str(), Entry.c_str());
      }
    }
    else if (action.startsWith("CRONDEL*")) {                     // Suppression une Entree, format CRONDEL*EntryKey
      String EntryKey = action.substring(8); crontab.deleteKey(EntryKey.c_str());
    }
    crontab.end();
  }
  else {
    Serial.println("Invalid Commande : "+commande);
  }
  return(1);
  }



void MyDelay (long duree) { unsigned long start = millis(); while ( millis()-start < duree ) {;} }  // replace delay()
void setup() {
  if (xTrace) { Trace=1; }
  Serial.begin(115200); delay(100); Serial.print("Début du setup, Version : "); Serial.println(version);
  pinMode(ledPin, OUTPUT);  // set the LED pin mode; 
  digitalWrite(ledPin, HIGH);
  Serial2.begin(115200, SERIAL_8N1, 4, 5);  // And configure Serial2 on pins RX=D4, TX=D5
  MyDelay(10);
  // We start by connecting to a WiFi network
  Serial.println(); Serial.println();
  Serial.print("Connecting to "); Serial.println(ssid);

  IPAddress ip(192, 168, 1, 99);
  WiFi.config(ip);              // url du serveur configuree a 192.198.1.99
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    MyDelay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  server.begin();
  
  // set notification call-back function
  //sntp_set_time_sync_notification_cb(timeavailable);
  //sntp_set_sync_interval(interval_time_update);  // sync every interval seconds
  // config time zone and NTP servers
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  setenv("TZ",time_zone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
  secondes=time(NULL);
  Serial.printf("Il s'est ecoule %f secondes depuis le 1er janvier 1970.\n", (double) secondes);

  digitalWrite(ledPin, LOW); Serial.println("module ESP32 WiFi : fin du Setup");
  // init de zone_message
  char buffer[80];
  strcpy(buffer,"Init;DISPLAY*Wifi: "); 
  strcat(buffer,version);
  ExecCommande(buffer);
  printLocalTime();
  listNvsKeys("crontab"); // charger la crontab en memoire => crontable
  //sprintf(buffer, "Init;DISPLAY*Wifi: %02d/%02d/%d %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  //ExecCommande(buffer);
  }
void loop() {
  nb_loop++; 
  NetworkClient client = server.accept();  // listen for incoming clients
  currentLine = "";          // make a String to hold incoming data from the client
  if (client) {                     // if you get a client,
    if (Trace==1) { Serial.println("New Client."); } // print a message out the serial port
    while (client.connected()) {    // loop while the client's connected
      if (client.available()) {     // if there's bytes to read from the client,
      // =============================================================================
        char c = client.read();     // read a byte, then
        //if (Trace==1) { Serial.write(c); }   // print it out the serial monitor
        if (c == '\n') {            // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            page_html_1(client);
            break;  // break out of the while loop:
          } else {  // if you got a newline, then process then clear currentLine:
            command_trt(currentLine); // process a GET line or ignore
            currentLine = "";
            //break;
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
        // Check to see if the client request was "GET /H" or "GET /L":
        else {
          //command_trt(currentLine);   // decode currentLine et exec action demandée
          //currentLine = "";
          }
      //===============================================================================
      }
    }
    // close the connection:
    client.stop();
    if (Trace==1) { Serial.println("Client Disconnected."); }
  }
  String cmd_recue = "";
  if (Serial.available()){         // lecture du port serie 0
    cmd_recue = serial_read(0); // serial reception
  }
  else if (Serial2.available()){         // lecture du port serie 2
    cmd_recue = serial_read(2); // serial2 reception format = "SRC;ACTION"
  }
  if ( cmd_recue!="" ) { 
    Serial.printf("loop: %d, ", nb_loop); Serial.println(cmd_recue); 
    int i=ExecCommande(cmd_recue);    // execution de la commande
    }
  MyDelay(100);
  
  ////////////////////////////////////////////////
  //                   CRONTAB
  ////////////////////////////////////////////////
  if (millis() - last_seconde>=1000)  {          // each seconde
    last_seconde=millis(); getLocalTime(&timeinfo);
  }

  if ( timeinfo.tm_min != last_minute && timeinfo.tm_sec==0 ) {           // each  minute
    last_minute=timeinfo.tm_min; 
    Serial.print("M: "); printLocalTime(); 
    time_t now = time(nullptr);                   // time sous forme de secondes
    //struct tm *currentTime = localtime(&now);   // pointeur de type structure date_time
    struct tm *currentTime = localtime(&now);   // pointeur de type structure date_time
    //
    //    CronTab
    //

    cron_exec(currentTime);

    for (const String& zcrontab : crontable) {    // parcourir la copie (crontable) memoire
      String zz = zcrontab; zz.replace("_"," ");     // remplacer les '_' par des ' '
      Serial.println(zz);
      int i = zz.indexOf(";");              // zcrontab contient la ligne courante

      if (isCronMatch(*currentTime, zz.substring(0,i))==true) {   // test crontab
          Serial.println("Cron match ! Exécuter : "+zz.substring(i+1));
          //int x=ExecCommande(zz.substring(i+1));      // execution commande standard format: "DEST;commande"   
          InputCommand=zz.substring(i+1); 
          if (InputCommand.startsWith("ZIGBEE;")) {     // traitement commande vers liason serie
            String champ=";"+InputCommand; Serial.println(champ);
            serial_write(champ);
          }
          else {
            ExecCommande(InputCommand);
          }
      } 
       
      //if (isCronMatch(*currentTime, "*/30 * * * * ")==true) {
      //    logLocalTime();                                                       // chaque 3 mn, logger date-time
      //    Serial.println("Cron : logLocalTime");
      //}
       
    }                       
  }
  // watchdog du réseau Serial2 (envoi un message ping tous les [DAISY_TEST_INTERVAL] secondes)
  test_daisy_chain_loop();
  }
