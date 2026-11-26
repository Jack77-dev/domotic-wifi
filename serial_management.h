/* doc: serial management
  22/10/2026, version 2.0 : intégration modification IA Gemini
    ajout daisy chain test
    optimisation du code (pour eviter les String pour protéger pb mémoire)
  01/09/2026, version 1.2 : correction bug sur test saturation, ajout delai < 1sec
  13/07/2026, version 1.1
    ajout filtrage/cleaning du message recu
 format :       D;25;DEST;EMET;Commande;F
                D is mandatory
                ; is the members separator (between each members)
                25 = total message length
                DEST : target device
                EMET : source device
                Command : text payload
                F is mandatory

  ESP32-C6 serial pin configuration :
  Serial1.begin(115200, SERIAL_8N1, 6, 7);  // Configure Serial1 on pins TX=D6 and RX=D7
  Serial2.begin(115200, SERIAL_8N1, 4, 5);  // And configure Serial2 on pins RX=D4, TX=D5
*/
// Variables globales pour le suivi du Daisy chain test
unsigned long test_sent_time = 0;
bool test_in_progress = false;
#define DAISY_TEST_INTERVAL 15000

/* doc : serial_read(int param)
  module de lecture des ports serie, peut lire serial, serial1, serial2 selon le parametre recu.
  - la lecture se fait jusqu'au caractere NewLine
  - seuls les caracteres imprimables (isprint) sont retenus, les autres sont ignores
  - si le meme message revient 10 fois de suite, il est ignore
  - le message lu doit respecter format "D;LNG;DST;SRC;Comaande;F", sinon il est ignore
  - 
*/
/* serial management - Ring / Loop Daisy Chain Mode (via Serial2)
  Format attendu : D;25;DEST;EMET;Commande;F
*/

#define TEST_MODE true        // Mode anneau actif
#define MAX_MSG_LEN 1024       
long nb_read = 0;
char last_msg_read[MAX_MSG_LEN] = ""; 
unsigned long last_read_time = 0; 
long nb_appels = 0; 
long nb_write = 0; 
int input_read(int param, char* output_buffer, size_t max_out_len);

String serial_read(int param) {
  char rx_buffer[1024];
  int result = input_read(param, rx_buffer, sizeof(rx_buffer));
  if (result>0) { return(String(rx_buffer)); }    // cas normal de reception d un message
  else if (result == -99) {                       // cas de retour du message de daisy chain test
      unsigned long send_time = 0;
      char *resultat = strrchr(rx_buffer, '_');
      send_time = String(resultat).substring(1).toInt();
      //Serial.printf("decodage message ping: %d, %d\n", send_time, (resultat-rx_buffer));
      if (resultat != NULL) {
        unsigned long round_trip_time = millis() - send_time;
        //Serial.printf("durée message ping:%lu ms.\n",round_trip_time);
        // On vérifie si c'est bien le test en cours (utile si plusieurs tours se croisaient)
        if (test_in_progress) {
          Serial.printf("✅ [TEST SUCCÈS] Boucle intègre ! Tour complet en %lu ms.\n", round_trip_time);
          test_in_progress = false; // On désactive le timeout, le test est réussi
        }
      }
      
  }
  return ( String("") ) ;   // autre cas: message vide ou invalide
}

int input_read(int param, char* output_buffer, size_t max_out_len) {
  output_buffer[0] = '\0';
  nb_appels++;

  String message_brut = "";
  switch (param) {
    case 0 : message_brut = Serial.readStringUntil('\n');  break;
    case 1 : message_brut = Serial1.readStringUntil('\n'); break; 
    case 2 : 
      if (TEST_MODE) {
        message_brut = Serial2.readStringUntil('\n'); 
      }
      break;
    default : 
      Serial.printf("param invalide : %d\n", param);
      return 0;
  }

  char message_lu[MAX_MSG_LEN];
  size_t clean_idx = 0;
  size_t length = message_brut.length();
  
  for (size_t i = 0; i < length && clean_idx < (MAX_MSG_LEN - 1); i++) {
    if (isprint(message_brut[i])) {
      message_lu[clean_idx++] = message_brut[i];
    }
  }
  message_lu[clean_idx] = '\0';

  if (clean_idx == 0) return 0;

  // Anti-saturation
  unsigned long current_time = millis();
  if (strcmp(last_msg_read, message_lu) == 0) {
    nb_read++;
    if (nb_read > 10 && (current_time - last_read_time) < 1000) {
      Serial.println("Error : serial saturation, message looping (>10)");
      return 0;
    }
  } else {
    nb_read = 1;
    strlcpy(last_msg_read, message_lu, sizeof(last_msg_read));
    last_read_time = current_time;
  }
  
  size_t lg = strlen(message_lu);
  if (lg < 6) return 0;

  char sep = message_lu[1];
  char* ptr = message_lu + 2; // ptr contient message
  
  char str_lng[8] = "";
  char DST[32] = "";
  char SRC[32] = "";

  // Extraction LNG
  char* next_sep = strchr(ptr, sep);
  if (!next_sep) return 0;
  memcpy(str_lng, ptr, next_sep - ptr);
  str_lng[next_sep - ptr] = '\0';
  ptr = next_sep + 1;

  // Extraction DST
  next_sep = strchr(ptr, sep);
  if (!next_sep) return 0;
  memcpy(DST, ptr, next_sep - ptr);
  DST[next_sep - ptr] = '\0';
  ptr = next_sep + 1;

  // Extraction SRC
  next_sep = strchr(ptr, sep);
  if (!next_sep) return 0;
  size_t len_src = next_sep - ptr;
  char* ptr_SRC = ptr;
  memcpy(SRC, ptr, len_src);
  SRC[len_src] = '\0';
  char* cmd_start = next_sep + 1;

  if (message_lu[lg - 1] != 'F') return 0;  // pas de flag F
  if ((int)lg != atoi(str_lng)) return 0;   // longeur lue incorrecte

  // --- GESTION DE LA BOUCLE (RING) ---

  // 1. Si le message vient de nous-mêmes (il a fait tout le tour de l'anneau) :
  if (strcmp(SRC, MySerialName) == 0) {
    memcpy(output_buffer, message_lu, lg-2);
    output_buffer[lg-2] = '\0';
    if (xTrace) {
      Serial.println("[RING] Message PING de retour.");
      //Serial.println(output_buffer);
    }
    // On peut lever un indicateur global ou simplement l'absorber pour qu'il ne re-tourne pas
    // C'est ici que s'arrête la vie du message dans l'anneau.
    return -99; // Code spécial pour indiquer "Mon propre message est de retour"
  }

  // 2. Si le message ne nous est pas destiné, on le propage obligatoirement dans l'anneau
  if (TEST_MODE) {
    if (strcmp(DST, MySerialName) != 0 && strcmp(DST, "*") != 0) {
      Serial2.print(message_lu);
      Serial2.print('\n');
      if (xTrace) { Serial.print("Propagation anneau (Serial2) : "); Serial.println(message_lu); }
      return 0; 
    }
  }

  // 3. Si le message nous est destiné ou destiné à tous (*)
  if (strcmp(DST, MySerialName) == 0 || strcmp(DST, "*") == 0) {
    char* last_sep_f = strrchr(cmd_start, sep);
    if (!last_sep_f) return 0;
    
    size_t cmd_len = last_sep_f - cmd_start;
    if (cmd_len >= max_out_len) cmd_len = max_out_len - 1;
    memcpy(output_buffer, ptr_SRC, len_src+1+cmd_len );                  // copy de SRC;payload
    output_buffer[len_src+1+cmd_len] = '\0';

    // Si c'est un broadcast (*), dans un anneau, on le consomme MAIS on le laisse aussi repartir 
    // pour que les voisins suivants le reçoivent (sauf si c'est déjà passé partout, géré par le tour complet).
    if (strcmp(DST, "*") == 0 && TEST_MODE) {
      Serial2.print(message_lu);
      Serial2.print('\n');
    }

    return (int)cmd_len+strlen(SRC)+1;
  }

  return 0;
}

void serial_write(String message) {
  nb_write++;
  // format attendu : ;DST;Commande
  if (xTrace) { Serial.print("Ecriture=");Serial.println(message); }
  long lg = message.length(); if (lg<2){ return; }
  char sep = message[0];
  long i; String DST=""; String SRC=""; String LNG=""; long SrcStart=0;
  for (i=1;i<lg & message[i] != sep; i++){ DST = DST+message[i]; } SrcStart=i+1;
  String Texte = sep+DST+sep+MySerialName+sep+message.substring(SrcStart)+sep+"F";
  if (xTrace) { Serial.printf("N° write: %d, ",nb_write); Serial.println(Texte); }
  // message checking
  if ( DST=="" ){ Serial.println("Error : DTS is null"); return;}
  long lgOut = (Texte+"D;00").length();
  if ( lgOut>99 ) { lgOut+=1; }
  Texte = "D;"+String(lgOut)+Texte+'\n';
  if (xTrace) { Serial.print("Output=");Serial.print(Texte); }
  Serial2.print(Texte);Serial2.flush();
}


// watch dog du serial2 (daisy chain)
void test_daisy_chain_loop() {
  unsigned long current_millis = millis();

  // Si un test est déjà en cours, on vérifie s'il y a timeout
  if (test_in_progress) {
    if (current_millis - test_sent_time > 2000) { // Timeout de 2 secondes
      Serial.println("❌ [TEST ERREUR] Timeout ! Le message n'est jamais revenu. La boucle est probablement coupée !");
      test_in_progress = false; // On libère le test pour en relancer un plus tard
    }
    return; // On attend que le test en cours se termine ou expire
  }

  // Lancer un nouveau test toutes les [DAISY_TEST_INTERVAL] secondes
  static unsigned long last_loop_test = 0;
  if (current_millis - last_loop_test < DAISY_TEST_INTERVAL) return;
  last_loop_test = current_millis;

  char msg_envoye[64];
  // On intègre le timestamp d'envoi dans le payload
  snprintf(msg_envoye, sizeof(msg_envoye), ";%s;PING_%lu", MySerialName, current_millis);
  
  serial_write(msg_envoye);
  
  test_sent_time = current_millis;
  test_in_progress = true; // On active le drapeau d'attente
  Serial.printf("--> [TEST] Ping envoyé (ID: %lu), en attente du tour complet...\n", current_millis);
}