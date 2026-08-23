# MediWatch NEO — Firmware

MediWatch NEO est la version de simulation du bracelet MediWatch pour la carte **NodeMCU-32S / ESP32** visible dans le projet.

## Principe

Le TMP117 manque dans le prototype. Il est donc remplace par un potentiometre sur `VP/GPIO36`.

La pression arterielle est simulee par un potentiometre sur `P35/GPIO35`.

L'AD8232 reste reel : son signal ECG analogique arrive sur `P34/GPIO34` et est affiche/acquis. Il n'est pas interprete medicalement.

Un troisieme potentiometre sur `VN/GPIO39` simule volontairement une frequence cardiaque anormale. Lorsque le potentiometre est normal, la FC du MAX30102 est utilisee si elle est valide. L'AD8232 continue toujours de fonctionner et son signal reste visible.

## Broches de la carte

```text
P21 / GPIO21  -> I2C SDA : OLED + MPU6050 + MAX30102
P22 / GPIO22  -> I2C SCL : OLED + MPU6050 + MAX30102
P34 / GPIO34  -> AD8232 OUT
VP  / GPIO36  -> POT-TMP
P35 / GPIO35  -> POT-PRESSION
VN  / GPIO39  -> POT-FC
P16 / GPIO16  -> GPS RX ESP32
P17 / GPIO17  -> GPS TX ESP32
P26 / GPIO26  -> GSM RX ESP32
P27 / GPIO27  -> GSM TX ESP32
P32 / GPIO32  -> bouton SOS
P14 / GPIO14  -> buzzer
P25 / GPIO25  -> RGB rouge
P33 / GPIO33  -> RGB vert
P13 / GPIO13  -> RGB bleu
```

## Logique FC

```text
POT-FC 50..120 BPM
    ↓
plage normale
    ↓
MAX30102 utilise pour la FC effective

POT-FC <50 ou >120 BPM
    ↓
anomalie volontaire
    ↓
POT-FC utilise comme FC effective
```

L'AD8232 n'est pas utilise comme compteur BPM dans cette version : il sert a l'acquisition ECG reelle et a verifier qu'un signal analogique est present.

## Regle stricte d'envoi SMS

Une anomalie seule ne suffit pas.

Le SMS automatique est autorise seulement si **5/5 conditions** sont vraies simultanement :

```text
MPU6050 anormal / chute
        AND
POT-TMP anormal
        AND
POT-PRESSION anormal
        AND
POT-FC anormal + AD8232 actif
        AND
MAX30102 anormal
        ↓
      5/5
        ↓
confirmation pendant 5 secondes
        ↓
SMS aux contacts + hopital
```

Le bouton SOS reste une exception et declenche un SMS immediat.

## Sans delay dans la boucle

La boucle principale utilise `millis()` pour les acquisitions, l'affichage et le buzzer. Aucun `delay()` n'est utilise dans `loop()`, afin de garder le GPS, le serveur Wi-Fi et les capteurs reactifs.

Les fonctions GSM utilisent toutefois des attentes temporisees lors des commandes AT, car le modem doit repondre avant de continuer l'envoi du SMS.

## Dashboard

L'ESP32 cree un point d'acces Wi-Fi :

```text
SSID : MEDIWATCH
Mot de passe : mediwatch123
Adresse habituelle : http://192.168.4.1
```

Le dashboard affiche notamment :

- temperature simulee
- pression simulee
- FC effective et sa source
- SpO2 experimental
- valeur ECG AD8232
- GPS
- les 5 conditions de la logique d'alerte
- etat NORMAL / WARNING / CRITICAL / SOS

## Bibliotheques

Installer dans Arduino IDE :

- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Sensor Library
- U8g2
- TinyGPSPlus

Le fichier `spo2_algorithm.h` doit etre disponible avec l'installation MAX3010x utilisee par le projet.

## Limites

Ce projet est un prototype pedagogique. Temperature, pression et FC sont simulees. Le signal ECG AD8232 n'est pas interprete. FC, SpO2 et detection de chute sont experimentaux. Ne pas utiliser pour un diagnostic, un traitement ou une decision medicale.
