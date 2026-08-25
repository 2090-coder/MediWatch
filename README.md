# MediWatch

**MediWatch — Bracelet de surveillance connectee base sur ESP32**

MediWatch est un prototype experimental destine a explorer l'acquisition de signaux, la simulation de certains parametres, la detection d'evenements et l'envoi d'alertes.

> **AVERTISSEMENT IMPORTANT :** MediWatch n'est pas un dispositif medical certifie. Temperature, pression et frequence cardiaque simulees par potentiometres ne sont pas des mesures medicales. L'ECG n'est pas interprete medicalement. FC, SpO2 et detection de chute sont experimentaux. Ne pas utiliser pour diagnostiquer ou traiter une personne.

## MediWatch NEO — architecture actuelle

La version NEO remplace le TMP117 absent par un potentiometre de temperature et ajoute deux potentiometres de simulation : pression et frequence cardiaque.

```text
                       MEDIWATCH NEO
                              |
                           ESP32
                              |
        +---------------------+----------------------+
        |                     |                      |
       I2C                   UART                  ANALOG
        |                     |                      |
   +----+-----+          +----+----+        +-------+--------+
   |    |     |          |         |        |       |        |
 MPU MAX  OLED          GPS       GSM    AD8232  POT-TMP  POT-PRESSION
                                            |               |
                                            +---- POT-FC ----+

                 + RGB + Buzzer + SOS
```

## Modules NEO

| Module | Role |
|---|---|
| ESP32 | controle principal |
| MPU6050 | mouvement / chute experimentale |
| MAX30102 | FC + SpO2 experimentaux |
| OLED SH1106 1.3" | affichage local |
| AD8232 | acquisition ECG reelle, sans interpretation medicale |
| Pot-TMP | simulation temperature, remplacement du TMP117 |
| Pot-pression | simulation pression arterielle |
| Pot-FC | simulation d'une FC anormale |
| GPS | localisation reelle |
| GSM | SMS |
| SOS | alerte manuelle |
| RGB + buzzer | alertes locales |

## Brochage NEO

| Fonction | ESP32 |
|---|---:|
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| AD8232 OUT | GPIO 34 |
| Pot-TMP | GPIO 36 |
| Pot-pression | GPIO 35 |
| Pot-FC | GPIO 39 |
| GPS RX | GPIO 16 |
| GPS TX | GPIO 17 |
| GSM RX | GPIO 26 |
| GSM TX | GPIO 27 |
| SOS | GPIO 32 |
| Buzzer | GPIO 14 |
| RGB Rouge | GPIO 25 |
| RGB Vert | GPIO 33 |
| RGB Bleu | GPIO 13 |

GPIO 21/22 sont le bus I2C partage par MPU6050, MAX30102 et OLED.

## Alimentation — Li-Po 3S

MediWatch NEO est alimente par une **Li-Po 3S 11,1 V / 1100 mAh**. Sa tension peut atteindre **12,6 V lorsqu'elle est completement chargee**.

La batterie n'est **jamais** branchee directement sur l'ESP32 ou le SIM800L. L'architecture utilise deux convertisseurs buck :

```text
Li-Po 3S 11,1 V / 12,6 V max
              |
        fusible + ON/OFF
              |
       +------+------+
       |             |
    Buck 1         Buck 2
    5,00 V          4,00 V
       |             |
    ESP32         SIM800L
    VIN/5V          VCC
       |             |
       +------GND---+
              |
         masse commune
```

- **Buck 1 : 5,00 V** → entrée VIN/5V de l'ESP32.
- **Buck 2 : 4,00 V** → alimentation dediee du SIM800L, capable de supporter ses pointes de courant.
- **3V3 de l'ESP32** → modules compatibles 3,3 V et potentiometres.
- **GND** → masse commune de tous les modules.
- Le **connecteur blanc 4 broches** de la batterie est le connecteur d'equilibrage 3S : il est reserve au chargeur Li-Po equilibre et ne sert pas a alimenter les modules.

Les sorties des deux bucks doivent etre reglees au **multimetre avant de connecter les modules**. Ne jamais envoyer 5 V sur la broche 3V3 et ne jamais envoyer 5 V ou 12,6 V directement au SIM800L.

Le cablage complet et la procedure de verification sont dans [`docs/power.md`](docs/power.md).

## Logique FC + AD8232

L'AD8232 reste reel et son signal analogique est acquis sur GPIO 34 et affiche sur le dashboard.

Le Pot-FC sert a provoquer volontairement une anomalie de frequence cardiaque :

- Pot-FC entre 50 et 120 BPM : plage normale ; la FC MAX30102 est utilisee comme FC effective si elle est valide.
- Pot-FC < 50 ou > 120 BPM : le Pot-FC devient la source effective pour simuler l'anomalie.
- L'AD8232 continue toujours d'etre acquis et affiche.

## Regle d'envoi automatique NEO

Une anomalie unique ne suffit **pas** pour envoyer un SMS automatique.

Le SMS automatique est autorise uniquement lorsque **les 5 conditions sont anormales simultanement** pendant 5 secondes :

```text
MPU6050 anormal / chute
        AND
Pot-TMP anormal
        AND
Pot-PRESSION anormal
        AND
Pot-FC anormal + AD8232 acquis
        AND
MAX30102 anormal
        = SMS D'URGENCE
```

Cette logique est volontairement stricte pour le prototype et reduit les faux declenchements.

### Exception SOS

Le bouton SOS reste independant de cette regle : une pression sur SOS provoque une alerte immediate.

## Pot-TMP

Le TMP117 n'est pas disponible. Le potentiometre de temperature est donc transforme par le firmware en temperature simulee.

Branchement :

```text
3V3 -> extremite 1
GND -> extremite 2
GPIO 36 -> curseur
```

## Pot-pression

```text
3V3 -> extremite 1
GND -> extremite 2
GPIO 35 -> curseur
```

La valeur est transformee en pression systolique/diastolique simulee.

## Pot-FC

```text
3V3 -> extremite 1
GND -> extremite 2
GPIO 39 -> curseur
```

Il permet de simuler volontairement 40 BPM, 180 BPM, etc., sans devoir provoquer une vraie anomalie physiologique.

## AD8232

```text
AD8232 OUTPUT -> GPIO 34
AD8232 GND    -> GND
AD8232 3.3V   -> 3V3
```

Les electrodes RA/LA/RL sont raccordees au breakout AD8232 selon son repere.

## GPS / GSM

```text
GPS TX -> GPIO 16
GPS RX -> GPIO 17

GSM TX -> GPIO 26
GSM RX -> GPIO 27
```

Le GSM doit avoir une alimentation adaptee a son modele et a ses pointes de courant. Ne jamais alimenter un modem GSM puissant depuis un GPIO.

## Dashboard

L'ESP32 cree un Wi-Fi local :

- SSID : `MEDIWATCH`
- Mot de passe : `mediwatch123`
- Dashboard : `http://192.168.4.1`

Endpoints :

- `/` — dashboard NEO
- `/api/data` — donnees temps reel
- `/api/status` — statut des modules

Le dashboard indique notamment quelle source de FC est utilisee et affiche les 5 conditions de la logique d'urgence.

## Arborescence

```text
MediWatch/
├── firmware/
│   ├── MediWatch_V1/
│   │   └── MediWatch_V1.ino
│   ├── MediWatch_V2/
│   │   ├── MediWatch_V2.ino
│   │   └── README.md
│   └── MediWatch_NEO/
│       ├── MediWatch_NEO.ino
│       └── README.md
├── hardware/
│   └── README.md
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   ├── wiring.md
│   └── power.md
└── README.md
```

## Bibliotheques NEO

- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Sensor Library
- TinyGPSPlus
- U8g2

`Wire`, `WiFi` et `WebServer` sont fournis par le core ESP32.

## Mise en route

1. Installer Arduino IDE et le core ESP32.
2. Installer les bibliotheques NEO.
3. Ouvrir `firmware/MediWatch_NEO/MediWatch_NEO.ino`.
4. Verifier le cablage dans `hardware/README.md` et `docs/power.md`.
5. Regler et verifier les deux bucks au multimetre : 5,00 V et 4,00 V.
6. Configurer les numeros `contact1`, `contact2`, `contact3` et `hospitalContact`.
7. Verifier l'alimentation du GSM.
8. Televerser.
9. Ouvrir le moniteur serie a 115200 bauds.
10. Se connecter au Wi-Fi `MEDIWATCH`.
11. Ouvrir `http://192.168.4.1`.

## Ordre de test

Tester progressivement :

1. ESP32 + OLED
2. I2C + MPU6050
3. MAX30102
4. Pot-TMP
5. Pot-pression
6. AD8232
7. Pot-FC
8. GPS
9. RGB + buzzer + SOS
10. GSM
11. Dashboard
12. Test des 5 conditions d'urgence

## Statut

**MediWatch NEO — nouvelle base de simulation du prototype.**
