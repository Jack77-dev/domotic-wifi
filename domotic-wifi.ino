#include <WiFi.h>
#include <WebServer.h>
#include <DomoticLink.h>
#include "wifi_config.h"

// NB: ne pas appeler la variable "link" (conflit avec la fonction POSIX link())
DomoticLink domoticLink;
WebServer server(80);

// Dernières trames reçues depuis le gateway Zigbee (Serial2)
String lastRxLog = "(aucune trame reçue)";

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='utf-8'><title>Domotic - IHM WiFi</title>";
    html += "<style>body{font-family:sans-serif;margin:2em}button{font-size:1.2em;padding:.5em 1em}</style>";
    html += "</head><body>";
    html += "<h1>IHM Domotique (ESP32-C6 WiFi)</h1>";
    html += "<p>IP locale : " + WiFi.localIP().toString() + "</p>";
    html += "<form action='/send' method='post'>";
    html += "<button type='submit'>Envoyer PING vers Zigbee</button>";
    html += "</form>";
    html += "<h2>Dernière trame reçue (Serial2)</h2>";
    html += "<pre>" + lastRxLog + "</pre>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleSend() {
    const char msg[] = "PING";
    domoticLink.send(0x01, (const uint8_t *)msg, 4);
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
}

void onFrame(const DomoticFrame &frame) {
    String s = "type=0x" + String(frame.type, HEX) + " len=" + String(frame.length) + " data=";
    for (uint8_t i = 0; i < frame.length; i++) {
        s += String(frame.payload[i], HEX);
        s += ' ';
    }
    lastRxLog = s;
    Serial.println("[Serial2 RX] " + s);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Domotic WiFi (ESP32-C6) ===");

    // Liaison filaire avec le gateway Zigbee
    domoticLink.begin(Serial2, 115200);
    domoticLink.setCallback(onFrame);

    // Connexion WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connexion WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connecte, IP: " + WiFi.localIP().toString());

    // Serveur web
    server.on("/", handleRoot);
    server.on("/send", HTTP_POST, handleSend);
    server.begin();
    Serial.println("Serveur web demarre sur port 80");
}

void loop() {
    server.handleClient();
    domoticLink.update();
}
