# domotic-wifi

Firmware de l'**ESP32-C6 #2** : IHM web en WiFi du projet domotique.

- Sert une page web (port 80) avec un bouton pour envoyer des commandes
- Communique avec l'autre ESP32-C6 (gateway Zigbee) via **Serial2** grâce à la bibliothèque `DomoticLink` ([domotic-common](https://github.com/Jack77-dev/domotic-common))

## Configuration

```bash
cp wifi_config.h.example wifi_config.h
# éditer wifi_config.h avec ton SSID / mot de passe WiFi
```

`wifi_config.h` est ignoré par git (identifiants jamais commités).

## Utilisation

1. Téléverser le sketch sur l'ESP32-C6 WiFi
2. Ouvrir l'IP affichée dans le moniteur série (115200 bauds) dans un navigateur
3. Cliquer sur le bouton → trame `0x01 "PING"` envoyée en Serial2 vers le gateway Zigbee

## Câblage Serial2 vers l'autre ESP32-C6

| ESP32-C6 WiFi (ce sketch) | ESP32-C6 Zigbee |
|---|---|
| TX Serial2 → | ← RX Serial2 |
| RX Serial2 → | ← TX Serial2 |
| GND → | ← GND |
