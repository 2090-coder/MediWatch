# Hardware — MediWatch NEO

## Carte utilisee

La carte de la photo est une **NodeMCU ESP-32S / ESP32S**, avec les broches imprimees sous forme `Pxx`, `VP` et `VN`.

Correspondance importante :

```text
VP = GPIO36
VN = GPIO39
P34 = GPIO34
P35 = GPIO35
P32 = GPIO32
P33 = GPIO33
P25 = GPIO25
P26 = GPIO26
P27 = GPIO27
P14 = GPIO14
P13 = GPIO13
P21 = GPIO21
P22 = GPIO22
P16 = GPIO16
P17 = GPIO17
```

## Brochage complet NEO

| Module / fonction | Broche visible sur la carte | GPIO ESP32 |
|---|---|---:|
| OLED SDA | P21 | GPIO21 |
| OLED SCL | P22 | GPIO22 |
| MPU6050 SDA | P21 | GPIO21 |
| MPU6050 SCL | P22 | GPIO22 |
| MAX30102 SDA | P21 | GPIO21 |
| MAX30102 SCL | P22 | GPIO22 |
| AD8232 OUT | P34 | GPIO34 |
| POT-TMP | VP | GPIO36 |
| POT-PRESSION | P35 | GPIO35 |
| POT-FC | VN | GPIO39 |
| GPS TX → ESP32 RX | P16 | GPIO16 |
| GPS RX ← ESP32 TX | P17 | GPIO17 |
| GSM TX → ESP32 RX | P26 | GPIO26 |
| GSM RX ← ESP32 TX | P27 | GPIO27 |
| Bouton SOS | P32 | GPIO32 |
| Buzzer | P14 | GPIO14 |
| LED RGB Rouge | P25 | GPIO25 |
| LED RGB Vert | P33 | GPIO33 |
| LED RGB Bleu | P13 | GPIO13 |

## Alimentation

Pour les capteurs 3,3 V :

```text
ESP32 3V3 → VCC des modules compatibles 3,3 V
ESP32 GND → GND commun
```

Le GSM doit avoir **son alimentation adaptee au modele exact**. Ne pas alimenter un module GSM depuis un GPIO de l'ESP32.

Toutes les masses doivent etre communes : ESP32, GPS, GSM, AD8232, OLED, MPU6050, MAX30102 et potentiometres.

## 1. Bus I2C

Les modules suivants partagent P21/P22 :

```text
ESP32 P21 / GPIO21 (SDA)
       ├── OLED SDA
       ├── MPU6050 SDA
       └── MAX30102 SDA

ESP32 P22 / GPIO22 (SCL)
       ├── OLED SCL
       ├── MPU6050 SCL
       └── MAX30102 SCL
```

VCC et GND de chaque module vont vers l'alimentation compatible du module.

## 2. POT-TMP — remplacement du TMP117

Le TMP117 n'est pas disponible dans NEO. Le potentiometre sur `VP/GPIO36` simule la temperature.

```text
POT-TMP
  extremite 1 → 3V3
  extremite 2 → GND
  curseur     → VP / GPIO36
```

Le programme transforme la position en environ **30 à 45 °C**.

Plage normale utilisee : **36,0 à 37,9 °C**.

## 3. POT-PRESSION

```text
POT-PRESSION
  extremite 1 → 3V3
  extremite 2 → GND
  curseur     → P35 / GPIO35
```

Le programme simule environ **70 à 200 mmHg systolique**.

La pression est une simulation pedagogique, pas une mesure arterielle.

## 4. AD8232 — ECG reel

L'AD8232 reste un vrai module.

```text
AD8232 3.3V    → ESP32 3V3
AD8232 GND     → ESP32 GND
AD8232 OUTPUT  → P34 / GPIO34
```

Les electrodes sont branchees normalement sur RA, LA et RL selon le module.

Le signal analogique est affiche/acquis. Le firmware ne fait **aucune interpretation medicale** de l'ECG.

## 5. POT-FC + AD8232

Le deuxieme potentiometre ne remplace pas l'AD8232. Il sert a creer volontairement une anomalie de frequence cardiaque pour la demonstration.

```text
POT-FC
  extremite 1 → 3V3
  extremite 2 → GND
  curseur     → VN / GPIO39
```

Le potentiometre simule environ **40 à 180 BPM**.

```text
50 à 120 BPM → plage normale
< 50 BPM     → anomalie
> 120 BPM    → anomalie
```

Quand le POT-FC est normal, le firmware utilise la FC du MAX30102 si elle est valide. L'AD8232 continue toujours d'acquerir son ECG.

Pour la condition d'urgence, il faut :

```text
POT-FC anormal
      ET
signal AD8232 acquis
```

## 6. MAX30102

```text
MAX30102 VCC → alimentation compatible
MAX30102 GND → GND
MAX30102 SDA → P21 / GPIO21
MAX30102 SCL → P22 / GPIO22
```

Le MAX30102 fournit experimentalement FC + SpO2.

## 7. MPU6050

```text
MPU6050 VCC → alimentation compatible
MPU6050 GND → GND
MPU6050 SDA → P21 / GPIO21
MPU6050 SCL → P22 / GPIO22
```

Une chute est detectee experimentalement par combinaison chute libre / impact.

## 8. GPS

Le TX du GPS va vers le RX de l'ESP32 et inversement :

```text
GPS TX → P16 / GPIO16
GPS RX → P17 / GPIO17
GPS GND → GND commun
GPS VCC → alimentation compatible
```

## 9. GSM

```text
GSM TX → P26 / GPIO26
GSM RX → P27 / GPIO27
GSM GND → GND commun
GSM VCC → alimentation adaptee au modele
```

Le GSM envoie les SMS aux proches et au contact hopital configure.

## 10. Bouton SOS

Le firmware utilise `INPUT_PULLUP` :

```text
P32 / GPIO32 ─── bouton ─── GND
```

```text
Relache → HIGH
Appuye  → LOW
```

Le SOS est une exception : **un SOS envoie immediatement l'alerte**, sans attendre la condition 5/5.

## 11. Buzzer

Pour un petit buzzer compatible GPIO :

```text
P14 / GPIO14 → BUZZER +
GND          → BUZZER -
```

Si le buzzer demande trop de courant, utiliser un transistor de commande et une alimentation adaptee.

## 12. LED RGB

Pour une RGB a cathode commune :

```text
P25 / GPIO25 → resistance → R
P33 / GPIO33 → resistance → G
P13 / GPIO13 → resistance → B
Cathode commune → GND
```

Utiliser une resistance par couleur.

## Regle stricte MEDIWATCH NEO

Le bracelet peut afficher une anomalie unique, mais le **SMS automatique est bloque** tant que les cinq conditions ne sont pas simultanement vraies :

```text
1. MPU6050 anormal / chute
             AND
2. POT-TMP hors plage normale
             AND
3. POT-PRESSION hors plage normale
             AND
4. POT-FC hors plage + AD8232 actif
             AND
5. MAX30102 anormal
             ↓
        5/5 CONDITIONS
             ↓
       confirmation 5 s
             ↓
          SMS GSM
```

## Ordre conseille de montage

1. ESP32 + alimentation
2. OLED + I2C P21/P22
3. MPU6050
4. MAX30102
5. POT-TMP sur VP
6. POT-PRESSION sur P35
7. AD8232 sur P34
8. POT-FC sur VN
9. GPS P16/P17
10. RGB P25/P33/P13 + resistances
11. Buzzer P14
12. SOS P32
13. GSM P26/P27 + alimentation adaptee

Tester chaque module avant de mettre en place la logique 5/5.
