# Documentation fonctionnelle — wifi-interface (ESP32-C6 WiFi)

Version du firmware : **V1.7 du 22/10/2026** (+ intégration NVS WiFi)

## 1. Rôle du module

L'ESP32-C6 WiFi est l'**interface homme-machine web** du projet domotique :
- il héberge un **serveur web HTTP** (port 80) accessible depuis le réseau local ;
- il communique avec l'autre ESP32-C6 (gateway **Zigbee**) via la liaison **Serial2** ;
- il gère l'**horloge** (NTP) et une **crontab** d'actions programmées.

## 2. Connexion WiFi (wifi_nvs.h)

| Situation | Comportement |
|---|---|
| Premier lancement (NVS vide) | Saisie de `SSID` puis `Mot de passe` sur le moniteur série (115200 bauds) |
| Connexion réussie | Identifiants **sauvegardés en NVS** (namespace `wifi`, clés `ssid` / `pass`) |
| Lancements suivants | Reconnexion **automatique** depuis la NVS, aucune saisie |
| Échec de connexion | NVS effacée (`clearCredentials`) → re-saisie au tour suivant |

- IP fixe configurée : **192.168.1.99** (`WiFi.config`)
- Timeout de connexion : 15 s

## 3. Serveur web (port 80)

Page d'accueil servie à chaque connexion client :

- **Lien « H »** (`GET /H`) : allume la LED builtin (`RGB_BUILTIN`)
- **Lien « L »** (`GET /L`) : éteint la LED builtin
- **Lien « Trace »** (`GET /Trace`) : bascule le mode trace (logs série détaillés)
- **Formulaire de commande** (`GET /submit-data?command=...`) : envoie une commande au format `SRC;ACTION`
  - si la commande commence par `>>` : envoyée telle quelle sur **Serial2** (préfixe `;`)
  - sinon : exécutée localement via `ExecCommande()`
- **Zone « Messages système »** : affiche les derniers messages (`zone_message`, tronquée à 2000 caractères)

## 4. Moteur de commandes `SRC;ACTION`

Les commandes reçues (web, port série USB `Serial`, ou `Serial2`) ont le format `SOURCE;ACTION` et sont traitées par `ExecCommande()` :

| Action | Effet |
|---|---|
| `REBOOT*` | Redémarre l'ESP32 (`ESP.restart()`) |
| `SETTIME*<epoch>` | Règle l'horloge système |
| `GETTIME*` | Renvoie l'heure courante au demandeur via Serial2 (`;SRC;SETTIME*<epoch>`) |
| `DISPLAY*<texte>` | Ajoute un message en tête de la zone « Messages système » de la page web |
| `DSPTIME*` | Logue l'heure locale sur le moniteur série |
| `CRONLIST*` | Liste les entrées de la crontab (NVS) |
| `CRONADD*<clé>/<entry>` | Ajoute/modifie une entrée crontab |
| `CRONDEL*<clé>` | Supprime une entrée crontab |

## 5. Liaison série avec le gateway Zigbee

- **Serial2** : `RX = GPIO4 (D4)`, `TX = GPIO5 (D5)`, 115200 bauds, 8N1
- Câblage croisé avec l'autre ESP32-C6 + **GND commun**
- Trames reçues au format `SRC;ACTION` → exécutées localement
- Commandes destinées au Zigbee envoyées via `serial_write()` (préfixe `;`)
- **Watchdog** : `test_daisy_chain_loop()` envoie un ping périodique sur Serial2 pour surveiller la liaison

### 5.1 Format technique des trames Serial2 (protocole « daisy chain » de `serial_management.h`)

Toutes les trames Serial2 suivent le format :

```
D;LNG;DST;SRC;COMMANDE;F\n
```

| Champ | Signification | Obligatoire |
|---|---|---|
| `D` | Flag de début de trame | oui |
| `LNG` | Longueur **totale** de la trame (préfixe `D;00` compris), sur 2 chiffres (3 si > 99) | oui |
| `DST` | Destinataire : nom du device (ex. `ZIGBEE`) ou `*` (broadcast) | oui |
| `SRC` | Émetteur : `MySerialName` (ex. `WIFI`) | oui |
| `COMMANDE` | Payload texte (ex. `DISPLAY*Hello`) | oui |
| `F` | Flag de fin de trame (dernier caractère avant `\n`) | oui |

- Séparateur entre champs : `;`
- Fin de ligne : `\n` (les caractères non imprimables sont filtrés à la réception)

**Exemple** — envoi de `;ZIGBEE;DISPLAY*Hello` depuis le module `WIFI` :
```
D;28;ZIGBEE;WIFI;DISPLAY*Hello;F\n
```

### 5.2 Émission (`serial_write`)

Entrée : `;DST;COMMANDE` (le `;` initial est le séparateur). La fonction construit la trame complète avec `SRC = MySerialName`, calcule `LNG`, ajoute `D`…`F\n`, et l'envoie sur Serial2.

### 5.3 Réception (`serial_read` / `input_read`)

1. Lecture jusqu'à `\n`, filtrage des caractères non imprimables
2. **Anti-saturation** : le même message reçu plus de 10 fois en moins d'1 s est ignoré
3. **Validations** : longueur ≥ 6, dernier caractère = `F`, longueur lue == `LNG`, champs correctement séparés — sinon trame ignorée
4. **Mode anneau (ring, `TEST_MODE = true`)** :
   - `SRC == MySerialName` → c'est notre propre message qui a fait le tour : il est absorbé (code retour `-99`)
   - `DST` ≠ nous et ≠ `*` → trame **re-propagée** sur Serial2
   - `DST` = nous ou `*` → extraction du payload `SRC;COMMANDE` remis à l'application

### 5.4 Watchdog de la liaison (daisy chain)

- Toutes les **15 s** (`DAISY_TEST_INTERVAL`), envoi d'un ping : `;WIFI;PING_<millis>` encapsulé dans une trame complète
- Si le ping ne revient pas dans les **2 s** → `Timeout ! La boucle est probablement coupée`
- Si le ping revient → calcul du **round-trip time** et log `Boucle intègre`

## 6. Crontab (programmation horaire)

- Stockée en **NVS** (namespace `crontab`) via `crontable.h` / `dictionary.h`
- Format d'une entrée : `<expression cron> ;<commande>`
  ex. `*/5 * * * * ;WIFI;DISPLAY*Ping toutes les 5 min`
- Évaluée **chaque minute** (quand `tm_sec == 0`)
- Si la commande de l'entrée commence par `ZIGBEE;` → envoyée sur Serial2, sinon exécutée localement

## 7. Horloge (NTP)

- Serveurs : `pool.ntp.org`, `time.nist.gov`
- Fuseau : `CET-1CEST,M3.5.0/2,M10.5.0/3` (heure d'été gérée)

## 8. Ports série

| Port | Broches | Usage |
|---|---|---|
| `Serial` (USB) | USB | Moniteur série, logs, saisie WiFi au 1er lancement, commandes `SRC;ACTION` |
| `Serial2` | RX=D4, TX=D5 | Liaison avec le gateway Zigbee |
