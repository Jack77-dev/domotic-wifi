# domotic-wifi

Firmware de l'**ESP32-C6 #2** : IHM web en WiFi du projet domotique.

Basé sur `SimpleWiFiServer_JC_V1_2`, renommé en `wifi-interface`.

## Structure

```
wifi-interface/
├── wifi-interface.ino    (sketch principal)
├── crontable.h
├── dictionary.h
├── serial_management.h   (liaison série avec l'autre ESP32-C6)
└── ci.json
```

> Pour l'IDE Arduino : ouvrir le dossier `wifi-interface/` (le nom du dossier
> doit correspondre au fichier .ino).

## Compilation

1. Cloner ce dépôt dans le sketchbook Arduino :
   `G:\Mon Drive\Caroli_Local\Arduino\MySketches\domotic-wifi`
2. Ouvrir `wifi-interface/wifi-interface.ino`
3. Sélectionner la carte ESP32-C6, Vérifier, Téléverser
