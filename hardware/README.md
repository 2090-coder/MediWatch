# Hardware — MediWatch NEO

## 1. Carte ESP32 utilisee

Carte : **NodeMCU ESP-32S / ESP32-WROOM**, avec les noms imprimes `Pxx`, `VP` et `VN`.

Correspondance exacte :

```text
VP  = GPIO36
VN  = GPIO39
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

Les GPIO34, 35, 36 et 39 sont des entrees uniquement : ils sont donc reserves aux signaux analogiques. citeturn0search1turn0search2

## 2. Tableau de cablage definitif

| Fonction | Module | Broche module | Broche ESP32 visible | GPIO |
|---|---|---|---|---:|
| I2C SDA | OLED SH1106 | SDA | P21 | GPIO21 |
| I2C SCL | OLED SH1106 | SCL | P22 | GPIO22 |
| I2C SDA | MPU6050 | SDA | P21 | GPIO21 |
| I2C SCL | MPU6050 | SCL | P22 | GPIO22 |
| I2C SDA | MAX30102 | SDA | P21 | GPIO21 |
| I2C SCL | MAX30102 | SCL | P22 | GPIO22 |
| ECG analogique | AD8232 | OUTPUT | P34 | GPIO34 |
| Temperature simulee | POT-TMP | curseur | VP | GPIO36 |
| Pression simulee | POT-PRESSION | curseur | P35 | GPIO35 |
| Frequence simulee | POT-FC | curseur | VN | GPIO39 |
| GPS RX ESP32 | NEO-6M-0-001 | TX | P16 | GPIO16 |
| GPS TX ESP32 | NEO-6M-0-001 | RX | P17 | GPIO17 |
| GSM RX ESP32 | SIM800L | TXD | P26 | GPIO26 |
| GSM TX ESP32 | SIM800L | RXD | P27 via adaptation 3.3V→2.8V | GPIO27 |
| SOS | bouton | contact | P32 | GPIO32 |
| Buzzer | buzzer | + | P14 | GPIO14 |
| RGB rouge | LED RGB | R | P25 via resistance | GPIO25 |
| RGB vert | LED RGB | G | P33 via resistance | GPIO33 |
| RGB bleu | LED RGB | B | P13 via resistance | GPIO13 |

## 3. Regle d'alimentation generale

Toutes les masses doivent etre communes :

```text
ESP32 GND
   ├── OLED GND
   ├── MPU6050 GND
   ├── MAX30102 GND
   ├── AD8232 GND
   ├── POT-TMP GND
   ├── POT-PRESSION GND
   ├── POT-FC GND
   ├── GPS GND
   ├── GSM GND
   ├── RGB cathode commune
   ├── buzzer GND
   └── alimentation GSM GND
```

Ne jamais utiliser un GPIO comme source d'alimentation d'un module.

## 4. Bus I2C

Le bus I2C est partage :

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

Pour les modules de breakout compatibles 3,3 V : VCC → 3V3 et GND → GND.

## 5. OLED SH1106 1,3 pouces

```text
OLED VCC → ESP32 3V3
OLED GND → ESP32 GND
OLED SDA → ESP32 P21
OLED SCL → ESP32 P22
```

Le firmware utilise le controleur SH1106 en I2C.

## 6. MPU6050

```text
MPU6050 VCC → ESP32 3V3
MPU6050 GND → ESP32 GND
MPU6050 SDA → ESP32 P21
MPU6050 SCL → ESP32 P22
```

La chute est une detection experimentale basee sur l'acceleration.

## 7. MAX30102

Pour un **breakout MAX30102 prevu pour 3,3 V** :

```text
MAX30102 VCC → ESP32 3V3
MAX30102 GND → ESP32 GND
MAX30102 SDA → ESP32 P21
MAX30102 SCL → ESP32 P22
```

Le MAX30102 fournit FC et SpO2 experimentalement. Le firmware ne presente pas ces valeurs comme un diagnostic medical.

## 8. AD8232 — ECG reel

```text
AD8232 3.3V   → ESP32 3V3
AD8232 GND    → ESP32 GND
AD8232 OUTPUT → ESP32 P34
```

Electrodes :

```text
RA → electrode RA
LA → electrode LA
RL → electrode RL
```

Le signal analogique est acquis et affiche. Il n'est pas interprete medicalement.

## 9. POT-TMP — remplacement du TMP117

Le TMP117 est absent du prototype NEO. Le premier potentiometre simule la temperature.

```text
POT-TMP
  borne 1  → ESP32 3V3
  borne 2  → ESP32 GND
  curseur  → ESP32 VP
```

`VP` correspond a GPIO36. Le firmware convertit 0–4095 en environ 30–45 °C.

Plage normale de demonstration :

```text
36.0 °C ≤ temperature < 38.0 °C  → normale
temperature < 36.0 °C             → anormale
temperature ≥ 38.0 °C             → anormale
```

## 10. POT-PRESSION — pression simulee

```text
POT-PRESSION
  borne 1  → ESP32 3V3
  borne 2  → ESP32 GND
  curseur  → ESP32 P35
```

P35 correspond a GPIO35.

Le firmware simule une pression systolique d'environ 70–200 mmHg.

```text
90 ≤ systolique < 140 mmHg → normale
systolique < 90            → anormale
systolique ≥ 140           → anormale
```

Ceci reste une **simulation pedagogique** et non une mesure de tension arterielle.

## 11. POT-FC + AD8232

Le deuxieme potentiometre ne remplace pas l'AD8232.

```text
POT-FC
  borne 1  → ESP32 3V3
  borne 2  → ESP32 GND
  curseur  → ESP32 VN
```

`VN` correspond a GPIO39.

Le potentiometre simule environ 40–180 BPM :

```text
50–120 BPM → normal
< 50 BPM   → anormal
> 120 BPM  → anormal
```

L'AD8232 continue simultanement a fournir le signal ECG reel.

La condition NEO numero 4 est strictement :

```text
POT-FC anormal
      ET
AD8232 signal present
```

Si le POT-FC est normal, la FC du MAX30102 est utilisee comme source de FC lorsqu'elle est valide.

## 12. GPS exact : u-blox NEO-6M-0-001

### Attention sur l'alimentation

Le **module u-blox NEO-6M-0-001 lui-meme** accepte une alimentation VCC de **2,7 à 3,6 V**. Donc, pour le module nu, utiliser **3,3 V**, jamais 5 V. citeturn0search36turn0search38

Si tu utilises une carte breakout NEO-6M avec regulateur, sa broche VCC peut avoir une plage differente selon la carte. Le brochage du breakout doit alors etre verifie sur sa carte avant de lui appliquer 5 V.

### UART exact

```text
NEO-6M TX → ESP32 P16 / GPIO16
NEO-6M RX ← ESP32 P17 / GPIO17
NEO-6M GND → ESP32 GND
NEO-6M VCC → ESP32 3V3 pour le module nu
```

C'est un croisement UART :

```text
GPS TX → ESP32 RX
GPS RX ← ESP32 TX
```

Le firmware utilise 9600 bauds.

### Antenne

Brancher l'antenne GPS sur le connecteur du breakout avant de tester la reception satellite.

## 13. GSM exact : SIM800L

### Alimentation — point critique

Le SIM800L exige **3,4 à 4,4 V**, avec **4,0 V recommande**, et peut demander jusqu'a **2 A en pointe pendant l'emission GSM**. Il ne faut donc pas l'alimenter depuis la sortie 3V3 de l'ESP32 ni depuis un GPIO. citeturn1search3turn1search4

Schema de principe :

```text
Alimentation dediee 4,0 V
        │
        ├──────── SIM800L VCC/VBAT
        │
       GND──────── SIM800L GND
        │
        └──────── ESP32 GND
```

Ajouter au plus pres du SIM800L un condensateur de decouplage de forte capacite. La documentation SIM800L recommande notamment un condensateur faible ESR de 100 µF sur VBAT ; une alimentation capable de tenir les pointes de courant est indispensable. citeturn1search24

**Ne pas utiliser 5 V directement sur le SIM800L.**

### UART exact

Le SIM800L utilise une logique UART autour de 2,8 V. Sa specification indique un niveau haut d'entree jusqu'a 3,1 V. Pour respecter proprement la conception recommandee, on adapte donc le TX 3,3 V de l'ESP32 vers l'entree RX du SIM800L. citeturn1search27turn2search12

```text
SIM800L TXD ─────────────────────→ ESP32 P26 / GPIO26

ESP32 P27 / GPIO27
       │
      1 kΩ
       │
       ├────────────────────────→ SIM800L RXD
       │
      5.6 kΩ
       │
      GND
```

Le montage 1 kΩ + 5,6 kΩ correspond au principe de matching UART 3,3 V documente par SIMCom et ramene environ 3,3 V vers 2,8 V. citeturn2search11

Donc :

```text
ESP32 P27 → 1 kΩ → SIM800L RXD
SIM800L RXD → 5.6 kΩ → GND
SIM800L TXD → ESP32 P26
```

Le RX de l'ESP32 recoit directement le TX du SIM800L.

### Antenne GSM

Connecter l'antenne GSM au connecteur d'antenne du SIM800L avant toute emission.

### Carte SIM

Inserer une carte SIM active avec service GSM/SMS et verifier que le module est bien enregistre sur le reseau avant de tester l'envoi de SMS.

## 14. Bouton SOS

```text
ESP32 P32 ─── bouton ─── GND
```

Le firmware utilise `INPUT_PULLUP` :

```text
Relache → HIGH
Appuye  → LOW
```

Le SOS est une exception : il declenche l'alerte manuelle sans attendre la condition 5/5.

## 15. Buzzer

Pour un petit buzzer compatible :

```text
ESP32 P14 → BUZZER +
ESP32 GND → BUZZER -
```

Si le buzzer demande plus de courant qu'un GPIO ne peut fournir, utiliser un transistor de commande.

## 16. LED RGB

Pour une LED RGB a cathode commune :

```text
ESP32 P25 → resistance → R
ESP32 P33 → resistance → G
ESP32 P13 → resistance → B
Cathode commune → GND
```

Une resistance est necessaire pour chaque couleur.

## 17. Regle d'urgence 5/5

Le bracelet peut afficher une anomalie unique, mais le SMS automatique est strictement bloque tant que les cinq conditions ne sont pas toutes vraies :

```text
1. MPU6050 anormal / chute
          AND
2. POT-TMP hors plage normale
          AND
3. POT-PRESSION hors plage normale
          AND
4. POT-FC hors plage + AD8232 signal present
          AND
5. MAX30102 anormal
          ↓
       5 / 5
          ↓
   confirmation 5 secondes
          ↓
       SMS GSM
```

Le GPS est utilise pour ajouter la position au SMS ; il n'est pas une des cinq conditions medicales.

## 18. Brochage final compact

```text
ESP32 NodeMCU-32S

P21  → SDA → OLED + MPU6050 + MAX30102
P22  → SCL → OLED + MPU6050 + MAX30102
P34  → ECG OUT → AD8232
VP   → POT-TMP
P35  → POT-PRESSION
VN   → POT-FC
P16  ← GPS TX
P17  → GPS RX
P26  ← SIM800L TXD
P27  → 1kΩ → SIM800L RXD
P32  → bouton SOS → GND
P14  → buzzer
P25  → RGB R + resistance
P33  → RGB G + resistance
P13  → RGB B + resistance
```

## 19. Ordre de montage et de test

1. ESP32 + alimentation USB
2. OLED + I2C P21/P22
3. MPU6050
4. MAX30102
5. POT-TMP sur VP
6. POT-PRESSION sur P35
7. AD8232 sur P34
8. POT-FC sur VN
9. GPS NEO-6M sur P16/P17
10. RGB + resistances
11. Buzzer
12. bouton SOS
13. alimentation dediee SIM800L à 4,0 V + masse commune
14. UART SIM800L P26/P27 avec adaptation 3,3 V→2,8 V
15. antenne GSM + SIM

Tester chaque bloc avant de tester la logique 5/5.

## 20. Important

MediWatch NEO est un prototype pedagogique. La temperature, la pression et la FC de demonstration sont simulees. L'ECG, la FC/SpO2, la detection de chute et la localisation sont experimentaux. Le systeme ne doit pas etre utilise pour un diagnostic ou une decision medicale reelle.
