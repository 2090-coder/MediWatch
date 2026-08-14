# MediWatch V2

Version 2 du firmware ESP32 du projet MediWatch.

## Architecture

- ESP32 : contrôleur principal
- TMP117 : température réelle
- MAX30102 : fréquence cardiaque + SpO2 expérimentale
- AD8232 : acquisition ECG analogique
- MPU6050 : détection expérimentale de chute
- Potentiomètre : simulation de pression artérielle
- GPS : position réelle
- GSM : SMS d'urgence
- OLED SH1106 1.3" : affichage local
- LED RGB + buzzer : alertes locales
- Bouton SOS : alerte manuelle
- Wi-Fi ESP32 : dashboard local

## Alertes V2

- Pression simulée >= 140 mmHg : WARNING
- Pression simulée >= 160 mmHg : CRITICAL après confirmation de 5 s
- Température >= 38 °C : WARNING
- Température >= 39 °C : CRITICAL
- SpO2 <= 94 % : WARNING
- SpO2 <= 90 % : CRITICAL
- FC < 50 ou > 120 BPM : WARNING
- Chute expérimentale : CRITICAL immédiat
- SOS : alerte immédiate

Une alerte GSM peut être envoyée aux trois proches et au contact de l'hôpital.

## Broches

| Fonction | ESP32 |
|---|---:|
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| AD8232 OUT | GPIO 34 |
| Potentiomètre | GPIO 35 |
| GPS RX | GPIO 16 |
| GPS TX | GPIO 17 |
| GSM RX | GPIO 26 |
| GSM TX | GPIO 27 |
| SOS | GPIO 32 |
| Buzzer | GPIO 14 |
| RGB R | GPIO 25 |
| RGB G | GPIO 33 |
| RGB B | GPIO 13 |

## Wi-Fi

- SSID : `MEDIWATCH`
- Mot de passe : `mediwatch123`
- Dashboard : `http://192.168.4.1`

## Bibliothèques

- Adafruit TMP117
- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Sensor Library
- TinyGPSPlus
- U8g2
- Le core ESP32 fournit Wire, WiFi et WebServer.

## Configuration avant essai

Modifier dans `MediWatch_V2.ino` :

```cpp
String contact1 = "+243XXXXXXXXX";
String contact2 = "+243XXXXXXXXX";
String contact3 = "+243XXXXXXXXX";
String hospitalContact = "+243XXXXXXXXX";
```

## Limites

La pression artérielle est uniquement simulée par potentiomètre. Le système n'est pas un dispositif médical certifié. L'ECG n'est pas interprété, la SpO2/FC sont expérimentales et la détection de chute est expérimentale.
