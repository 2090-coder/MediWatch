# MediWatch

**Smart Medical Monitoring Bracelet — ESP32**

MediWatch est un prototype de bracelet connecté destiné à la surveillance de paramètres physiologiques et à la gestion d'alertes d'urgence.

> **Prototype uniquement.** Les valeurs et seuils de ce projet ne constituent pas un dispositif médical certifié et ne doivent pas servir au diagnostic ou au traitement d'une personne.

## Fonctionnalités V1

- TMP117 : température
- MAX30102 : fréquence cardiaque et SpO₂
- AD8232 : lecture ECG analogique
- MPU6050 : mouvement et détection expérimentale de chute
- Potentiomètre : simulation de pression systolique/diastolique
- GPS : latitude/longitude
- GSM : SMS d'urgence vers 3 contacts
- Bouton SOS
- LED RGB + buzzer
- OLED SH1106 128×64
- Dashboard Web hébergé directement par l'ESP32
- Mode Wi-Fi Access Point pour une utilisation locale sans Node.js

## Architecture

```text
Capteurs → ESP32 → traitement →
          ├── OLED / LED / Buzzer
          ├── GSM → SMS d'urgence
          ├── GPS → position
          └── Wi-Fi → Dashboard Web
```

## Brochage V1

| Fonction | ESP32 |
|---|---:|
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| AD8232 OUT | GPIO 34 |
| Potentiomètre pression | GPIO 35 |
| GPS RX | GPIO 16 |
| GPS TX | GPIO 17 |
| GSM RX | GPIO 26 |
| GSM TX | GPIO 27 |
| SOS | GPIO 32 |
| Buzzer | GPIO 14 |
| LED Rouge | GPIO 25 |
| LED Verte | GPIO 33 |
| LED Bleue | GPIO 13 |

## Wi-Fi

Au démarrage, l'ESP32 crée le réseau :

- **SSID :** `MEDIWATCH`
- **Mot de passe :** `mediwatch123`
- **Dashboard :** `http://192.168.4.1`

Le mot de passe est volontairement simple pour le prototype et doit être remplacé avant toute utilisation réelle.

## API Web

- `GET /` → dashboard
- `GET /api/data` → données JSON du bracelet
- `GET /api/ecg` → valeur ECG instantanée
- `GET /api/status` → état de connexion du système

## Structure

```text
MediWatch/
├── firmware/
│   └── MediWatch_V1/
│       └── MediWatch_V1.ino
├── web/
│   └── index.html
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   └── wiring.md
├── hardware/
│   └── README.md
├── .gitignore
├── LICENSE
└── README.md
```

## Bibliothèques Arduino

Installer :

- Adafruit TMP117
- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Sensor Library
- TinyGPSPlus
- U8g2

Les bibliothèques `Wire`, `WiFi` et `WebServer` sont fournies par le core ESP32.

## Mise en route

1. Installer le core ESP32 dans Arduino IDE.
2. Installer les bibliothèques ci-dessus.
3. Ouvrir `firmware/MediWatch_V1/MediWatch_V1.ino`.
4. Sélectionner une carte ESP32 compatible.
5. Vérifier les broches et l'alimentation des modules.
6. Modifier les trois numéros GSM dans le fichier avant un essai réel.
7. Téléverser.
8. Se connecter au Wi-Fi `MEDIWATCH`.
9. Ouvrir `http://192.168.4.1`.

## Limites connues

La pression artérielle est **simulée par potentiomètre**. Elle n'est pas mesurée par un capteur de pression artérielle. La détection de chute est expérimentale. Le calcul SpO₂/FC dépend du positionnement correct du doigt sur le MAX30102 et de la qualité du signal.

## Licence

Projet fourni pour expérimentation et apprentissage. Voir `LICENSE`.
