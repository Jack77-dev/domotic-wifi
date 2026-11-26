/*
Module de gestion Dictionary NVS
22/08/2026 : V1.0 : creation du module

utilisation :

  Dictionary myDict("Mon_Dictionnaire");  Pour creer l'objet "Mon_Dictionnaire"
  myDict.begin();                         Pour ouvrir le dictionnaire
  myDict.end();                           Pour fermer le dictionnaire

  myDict.putKey("ma cle", "valeur cle");  Pour créer/modifier une cle dans le dictionnaire, longueur maxi de cle = 15 char
  myDict.deleteKey("cle a supprimer");    Pour supprimre la clé

  listNvsKeys("Mon_Dictionnaire");        Pour lister le dictionnaire
  myDict.clearAll()                       Pour supprimer toutes les entrees du dictionnaire

*/
#include <Preferences.h>
#include "nvs_flash.h"
#include "nvs.h"
// declarations globales
extern void logmessage(String zMessage);
extern std::vector<String> crontable;

class Dictionary {
private:
  Preferences prefs;
  String _namespaceName;
public:
  // Constructeur : définit le nom du sous-dossier (namespace) en NVS
  Dictionary(const char* namespaceName) {
    _namespaceName = namespaceName;
  }
  // Initialisation du stockage
  bool begin() {
    return prefs.begin(_namespaceName.c_str(), false); // false = mode lecture/écriture
  }
  // Fermeture explicite
  void end() {
    prefs.end();
  }

  // 1. ADD/REPLACE : Ajoute une nouvelle clé ou replace un clé existante
  bool putKey(const char* key, const String& value) {
    prefs.putString(key, value);
    return true;
  }
  
  // 3. DELETE : Supprime une clé si elle existe
  bool deleteKey(const char* key) {
    prefs.remove(key);
    return true;
  }
  // Supprime TOUTES les clés du dictionnaire
  void clearAll() {
    prefs.clear();
    //Serial.println("[DICT] Dictionnaire entièrement réinitialisé !");
  }
  // Méthodes de lecture (Getters) avec valeur par défaut
  String getString(const char* key, String defaultValue = "") {
    return prefs.getString(key, defaultValue);
  }
  // Vérifier si une clé existe
  bool contains(const char* key) {
    return prefs.isKey(key);
  }
};

// Fonction pour lister les clés associées à un namespace donné
void listNvsKeys(const char* targetNamespace) {
  Preferences prefs;
  String message_err="[NVS] Aucune clé trouvée ou namespace "+String(targetNamespace)+" introuvable.";
  String message_entete="====== Liste du dictionnaire : "+String(targetNamespace);
  nvs_iterator_t it = NULL;
  it = NULL;
  // Initialise l'itérateur NVS pour le type NVS_TYPE_ANY (tous les types de données)
  esp_err_t res = nvs_entry_find(NVS_DEFAULT_PART_NAME, targetNamespace, NVS_TYPE_ANY, &it);
  if (res != ESP_OK) {
    Serial.println(message_err);
    logmessage(message_err);
    return;
  }
  Serial.printf("\n--- Clés trouvées dans le namespace '%s' ---\n", targetNamespace);
  prefs.begin(targetNamespace, true);       // ouverture du dictionnaire en lecture seule
  Serial.println(message_entete);    // Affiche ligne entete
  crontable.clear();                // vider la table crontable
  while (it != NULL) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info); // Extrait les informations de l'entrée courante
    crontable.push_back(prefs.getString(info.key).c_str()); // copie crontab dans la table courante crontable
    char buffer[80]; sprintf(buffer, "%-15s: %-15s | %s", targetNamespace, info.key, prefs.getString(info.key).c_str());
    //Serial.printf("%-15s | %-15s | %s\n", targetNamespace, info.key, prefs.getString(info.key).c_str());  // Affiche la ligne de donnees
    Serial.println(buffer);
    logmessage(String(buffer));
    // Passe à l'entrée suivante
    res = nvs_entry_next(&it);
  }
  logmessage(message_entete);   // logger la ligne entete en dernier
  // Libère la mémoire attribuée à l'itérateur
  nvs_release_iterator(it);
  prefs.end();
  Serial.println("----------------------------------------------\n");
}


