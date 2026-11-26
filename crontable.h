/*
Module de gestion de la fonction crontable

23/10/2026, version 1.0 : creation du module

*/
extern std::vector<String> crontable;
int  parseCronField(const String &field, const struct tm &currentTime, int fieldIndex);

  ////////////////// 2 fonctions de CRONTAB //////////////////
bool isCronMatch(const struct tm &currentTime, const String &cronLine) {  
  // Extraire les 5 champs de la ligne crontab (séparés par des espaces)
  //Serial.print("cronLine: ");Serial.println(cronLine);
  int cronFields[5]; int fieldIndex = 0; String fieldBuffer;
  //
  for (char c : cronLine) {
    if (c == ' ' || c == '\t') {
      if (fieldBuffer.length() > 0) {
        cronFields[fieldIndex] = parseCronField(fieldBuffer, currentTime, fieldIndex);
        fieldBuffer = ""; fieldIndex++;
      }
      if (fieldIndex >= 5) break; // On a tout extrait
    } else {
      fieldBuffer += c;
    }
  }
  if (fieldBuffer.length() > 0 && fieldIndex < 5) { cronFields[fieldIndex] = parseCronField(fieldBuffer, currentTime, fieldIndex); } // dernier champ
  //
  Serial.printf("cronfields: %d %d %d %d %d\n",cronFields[0],cronFields[1],cronFields[2],cronFields[3],cronFields[4]);
  Serial.printf("currentime: %d %d %d %d %d\n",currentTime.tm_min,currentTime.tm_hour,currentTime.tm_mday,(currentTime.tm_mon+1),currentTime.tm_wday);
  // Vérifier si tous les champs correspondent
  bool retour = true;
  if ( cronFields[0] != currentTime.tm_min ){ retour=false; Serial.println("Minute mismatch");}              // Minute
  else if (cronFields[1] != currentTime.tm_hour ){ retour=false; Serial.println("Heure mismatch");}          // Heure
  else if (cronFields[2] != currentTime.tm_mday ){ retour=false; Serial.println("Jour du mois mismatch");}   // Jour du mois
  else if (cronFields[3] != currentTime.tm_mon + 1 ){ retour=false; Serial.println("Mois mismatch");}        // Mois (tm_mon est 0-based)
  else if (cronFields[4]!= currentTime.tm_wday) { retour=false; Serial.println("Jour semaine mismatch");}    // Jour de la semaine
  Serial.println(retour ? "true" : "false");
  return (retour);
  }

int  parseCronField(const String &field, const struct tm &currentTime, int fieldIndex) { // Fonction auxiliaire pour parser un champ crontab (ex: "*", "5", "1-5", "*/2", etc.)
  // Implémenter la logique pour chaque type de champ (*, liste, plage, step)
  int temps[5]; 
  temps[0]=currentTime.tm_min; temps[1]=currentTime.tm_hour; temps[2]=currentTime.tm_mday; 
  temps[3]=currentTime.tm_mon+1; temps[4]=currentTime.tm_wday; temps[5]=currentTime.tm_sec;
  //Serial.printf("temps: %d %d %d %d %d\n",temps[0],temps[1],temps[2],temps[3],temps[4]);
  //Serial.print("parse: "); Serial.print(field); Serial.printf(" %d",fieldIndex);
  //
  if (field == "*") { return temps[fieldIndex];   }    // '*' toujours vrai, retourner le champ time
  else if (field.startsWith("*/")) {                   // '*/' calculer un interval
    int i = temps[fieldIndex] % field.substring(2).toInt();     //Serial.printf("modulo=%d\n",i);
    if (i==0) {return temps[fieldIndex]; }
    else { return (int)-1;}
  }
  else {return field.toInt();}                          // retouner la valeur du champ cronline
  // Ajouter la logique pour les autres cas (nombres, plages, listes, steps)
  // ...
  }

void cron_exec(tm* currentTime) {
for (const String& zcrontab : crontable) { 
    // 1. Trouver directement le séparateur du cron (ex: "*/5 * * * * ;COMMANDE")
    int i = zcrontab.indexOf(';');
    //if (i < 0) continue; // Sécurité : ligne invalide si pas de ';'

    // 2. Extraire la partie planification (cron expression) directement sans dupliquer toute la ligne
    // On remplace les '_' par des ' ' uniquement sur une portion temporaire ou à la volée si besoin,
    // ou plus simple : on extrait la sous-chaîne du cron et on la nettoie.
    String cronExpr = zcrontab.substring(0, i);
    cronExpr.replace('_', ' ');

    // 3. Test du match cron
    if (isCronMatch(*currentTime, cronExpr)) { 
        // Extraire la commande (tout ce qui suit le premier ';')
        String inputCommand = zcrontab.substring(i + 1);
        
        // Remplacer les '_' par des ' ' dans la commande si elle en contient (selon votre logique initiale)
        inputCommand.replace('_', ' ');

        Serial.print("Cron match ! Exécuter : ");
        Serial.println(inputCommand);

        // 4. Traitement selon le type de commande
        if (inputCommand.startsWith("ZIGBEE;")) { 
            // Éviter de recréer un objet String juste pour ajouter un ';' si serial_write attend le format correct
            // Si serial_write attend un format avec séparateur au début :
            String champ = ";" + inputCommand; 
            Serial.println(champ);
            serial_write(champ);
        } else {
            ExecCommande(inputCommand);
        }
      } 
                       
    }
}

void crontab_old(tm* currentTime){
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