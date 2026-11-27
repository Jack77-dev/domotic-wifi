#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

namespace wifi_nvs {

constexpr const char *NVS_NAMESPACE   = "wifi";
constexpr const char *NVS_KEY_SSID    = "ssid";
constexpr const char *NVS_KEY_PASS    = "pass";
constexpr uint32_t    CONNECT_TIMEOUT = 15000;

// Lit les identifiants en NVS. Retourne true si ssid et pass sont renseignes.
inline bool readCredentials(String &ssid, String &pass) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  ssid = prefs.getString(NVS_KEY_SSID, "");
  pass = prefs.getString(NVS_KEY_PASS, "");
  prefs.end();
  return ssid.length() > 0 && pass.length() > 0;
}

// Sauvegarde les identifiants en NVS.
inline void saveCredentials(const String &ssid, const String &pass) {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_SSID, ssid);
  prefs.putString(NVS_KEY_PASS, pass);
  prefs.end();
}

// Efface les identifiants en NVS (force la re-saisie au prochain appel).
inline void clearCredentials() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
}

// Saisie des identifiants sur le moniteur serie.
inline void promptCredentials(String &ssid, String &pass) {
  Serial.setTimeout(60000);

  Serial.print("SSID : ");
  while (Serial.available()) Serial.read();
  while (!Serial.available()) delay(10);
  ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.print("Mot de passe : ");
  while (!Serial.available()) delay(10);
  pass = Serial.readStringUntil('\n');
  pass.trim();
}

// Connexion WiFi : identifiants NVS si presents, sinon saisie manuelle.
// Sauvegarde en NVS uniquement si la connexion reussit.
inline bool connectWiFi() {
  String ssid, pass;
  bool fromNvs = readCredentials(ssid, pass);

  if (!fromNvs) {
    Serial.println("Aucun identifiant en NVS, saisie manuelle :");
    promptCredentials(ssid, pass);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < CONNECT_TIMEOUT) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Echec de connexion WiFi");
    return false;
  }

  Serial.print("Connecte, IP : ");
  Serial.println(WiFi.localIP());

  if (!fromNvs) saveCredentials(ssid, pass);
  return true;
}

} // namespace wifi_nvs
