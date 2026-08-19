# MediWatch V2

Firmware principal du prototype MediWatch basé sur ESP32.

## Rôle

Le firmware regroupe l'acquisition des capteurs, la logique d'état, l'affichage OLED, les alertes locales, le GPS, le GSM et le dashboard Web local.

## Modules

- **TMP117** → température réelle
- **MPU6050** → mouvement et détection expérimentale de chute
- **MAX30102** → fréquence cardiaque + SpO₂ expérimentale
- **AD8232** → acquisition ECG analogique
- **Potentiomètre** → simulation de pression artérielle
- **GPS** → latitude/longitude réelles
- **GSM** → SMS d'urgence
- **OLED SH1106 1,3"** → affichage local
- **LED RGB** → indication d'état
- **Buzzer** → alerte sonore
- **Bouton SOS** → alerte manuelle
- **Wi-Fi ESP32** → dashboard local

## Broches

| Fonction | GPIO |
|---|---:|
| I²C SDA | 21 |
| I²C SCL | 22 |
| AD8232 OUT | 34 |
| Potentiomètre | 35 |
| GPS RX | 16 |
| GPS TX | 17 |
| GSM RX | 26 |
| GSM TX | 27 |
| SOS | 32 |
| Buzzer | 14 |
| RGB Rouge | 25 |
| RGB Vert | 33 |
| RGB Bleu | 13 |

TMP117, MPU6050, MAX30102 et OLED partagent le bus I²C 21/22.

## États

```text
NORMAL
  │
  ├── warning sensoriel ──────> WARNING
  │
  ├── pression simulée critique
  │       └── confirmation 5 s ─> CRITICAL
  │
  ├── chute ──────────────────> CRITICAL
  │
  └── SOS ────────────────────> SOS
```

### Seuils du prototype

- Pression simulée ≥ 140 mmHg → WARNING
- Pression simulée ≥ 160 mmHg pendant 5 s → CRITICAL
- Température ≥ 38 °C → WARNING
- Température ≥ 39 °C → CRITICAL
- SpO₂ ≤ 94 % → WARNING
- SpO₂ ≤ 90 % → CRITICAL
- FC < 50 ou > 120 BPM → WARNING
- Chute expérimentale → CRITICAL immédiat
- SOS → alerte immédiate

Ces seuils sont des paramètres de démonstration et ne constituent pas des recommandations médicales.

## GSM

Le firmware prépare un SMS contenant :

- patient ;
- motif de l'alerte ;
- pression **SIMULÉE** ;
- FC si valide ;
- SpO₂ si valide ;
- température ;
- lien Google Maps si la position GPS est valide.

Destinataires configurables dans le code :

```cpp
String contact1 = "+243XXXXXXXXX";
String contact2 = "+243XXXXXXXXX";
String contact3 = "+243XXXXXXXXX";
String hospitalContact = "+243XXXXXXXXX";
```

Remplacer les valeurs avant un test GSM.

## Wi-Fi

```text
SSID       : MEDIWATCH
Password   : mediwatch123
Dashboard  : http://192.168.4.1
```

Endpoints :

```text
GET /api/data
GET /api/status
```

## Bibliothèques

- Adafruit TMP117
- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Sensor Library
- TinyGPSPlus
- U8g2

Le core ESP32 fournit `Wire`, `WiFi` et `WebServer`.

## Installation

1. Installer le core ESP32 dans Arduino IDE.
2. Installer les bibliothèques ci-dessus.
3. Ouvrir `MediWatch_V2.ino`.
4. Choisir la carte ESP32 correspondant au matériel.
5. Vérifier le câblage dans `hardware/README.md` ou `docs/wiring.md`.
6. Configurer les numéros GSM.
7. Téléverser.
8. Ouvrir le moniteur série à 115200 bauds.
9. Connecter un appareil au Wi-Fi `MEDIWATCH`.
10. Ouvrir `http://192.168.4.1`.

## Limites

MediWatch V2 est un prototype d'apprentissage et de démonstration :

- la pression artérielle est simulée ;
- l'ECG est acquis mais non interprété ;
- la FC et la SpO₂ sont expérimentales ;
- la détection de chute est expérimentale ;
- le GPS dépend de la réception satellite ;
- le GSM dépend du réseau, de la SIM et de son alimentation.

**Ne pas utiliser MediWatch V2 pour un diagnostic ou un traitement médical.**
