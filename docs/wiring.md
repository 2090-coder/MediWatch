# MediWatch NEO — Câblage complet

## 1. Vue générale

```text
                         ┌──────────────────┐
                         │       ESP32       │
                         │    NodeMCU-32S    │
                         └────────┬─────────┘
                                  │
          ┌───────────────────────┼──────────────────────┐
          │                       │                      │
         I²C                     UART                  ADC/GPIO
          │                       │                      │
   ┌──────┼───────┐        ┌─────┴─────┐        ┌──────┴─────────┐
   │      │       │        │           │        │       │        │
 MPU   MAX30102 OLED      GPS         GSM     AD8232 POT-TMP POT-PRESS
                                                │             │
                                                │           POT-FC
                                                │
                                  + RGB + Buzzer + SOS
```

Le TMP117 est absent du prototype NEO : il est remplace par POT-TMP.

## 2. Tableau de référence

| Fonction | Module | Broche ESP32 visible | GPIO |
|---|---|---:|---:|
| I2C SDA | OLED + MPU6050 + MAX30102 | P21 | GPIO21 |
| I2C SCL | OLED + MPU6050 + MAX30102 | P22 | GPIO22 |
| ECG analogique | AD8232 OUT | P34 | GPIO34 |
| Temperature simulee | POT-TMP curseur | VP | GPIO36 |
| Pression simulee | POT-PRESSION curseur | P35 | GPIO35 |
| FC simulee | POT-FC curseur | VN | GPIO39 |
| RX GPS ESP32 | GPS TX | P16 | GPIO16 |
| TX GPS ESP32 | GPS RX | P17 | GPIO17 |
| RX GSM ESP32 | GSM TXD | P26 | GPIO26 |
| TX GSM ESP32 | GSM RXD via adaptation | P27 | GPIO27 |
| SOS | bouton vers GND | P32 | GPIO32 |
| Buzzer | entree commande | P14 | GPIO14 |
| Rouge | LED RGB via resistance | P25 | GPIO25 |
| Vert | LED RGB via resistance | P33 | GPIO33 |
| Bleu | LED RGB via resistance | P13 | GPIO13 |

Les GPIO34, 35, 36 et 39 sont des entrees uniquement. Ils sont reserves ici aux signaux analogiques.

## 3. Alimentation générale

La batterie utilisee est une **Li-Po 3S 11,1 V / 1100 mAh**, pouvant atteindre **12,6 V chargee**.

```text
Li-Po 3S
   │
   ├── + → fusible → interrupteur → + Buck 5 V
   │                              └→ + Buck 4 V
   │
   └── - ─────────────────────────→ masses / GND commun

Buck 5 V → ESP32 VIN/5V
Buck 4 V → SIM800L VCC/VBAT
ESP32 3V3 → modules compatibles 3,3 V
```

**La Li-Po ne doit jamais etre branchee directement sur l'ESP32, le SIM800L ou un ADC.**

Le cablage detaille, les reglages au multimetre et le role du connecteur d'equilibrage sont documentes dans [`power.md`](power.md).

## 4. I²C partage

```text
ESP32 P21 / GPIO21 SDA
       ├── OLED SDA
       ├── MPU6050 SDA
       └── MAX30102 SDA

ESP32 P22 / GPIO22 SCL
       ├── OLED SCL
       ├── MPU6050 SCL
       └── MAX30102 SCL
```

Les modules I²C doivent avoir des adresses compatibles et distinctes lorsqu'elles sont necessaires.

## 5. OLED SH1106 1,3 pouces

```text
OLED VCC → ESP32 3V3
OLED GND → ESP32 GND
OLED SDA → ESP32 P21
OLED SCL → ESP32 P22
```

## 6. MPU6050

```text
MPU6050 VCC → ESP32 3V3
MPU6050 GND → ESP32 GND
MPU6050 SDA → ESP32 P21
MPU6050 SCL → ESP32 P22
```

La detection de chute reste experimentale.

## 7. MAX30102

Pour un breakout explicitement compatible 3,3 V :

```text
MAX30102 VCC → ESP32 3V3
MAX30102 GND → ESP32 GND
MAX30102 SDA → ESP32 P21
MAX30102 SCL → ESP32 P22
```

## 8. AD8232 — ECG reel

```text
AD8232 3.3V   → ESP32 3V3
AD8232 GND    → ESP32 GND
AD8232 OUTPUT → ESP32 P34
```

Le signal ECG est acquis et affiche mais n'est pas interprete medicalement.

## 9. POT-TMP — remplacement du TMP117

```text
POT-TMP borne 1 → ESP32 3V3
POT-TMP borne 2 → ESP32 GND
POT-TMP curseur → ESP32 VP / GPIO36
```

Le firmware transforme la valeur ADC en temperature simulee.

## 10. POT-PRESSION

```text
POT-PRESSION borne 1 → ESP32 3V3
POT-PRESSION borne 2 → ESP32 GND
POT-PRESSION curseur → ESP32 P35 / GPIO35
```

La pression systolique/diastolique est simulee par le firmware.

## 11. POT-FC + AD8232

Le potentiometre ne remplace pas l'AD8232.

```text
POT-FC borne 1 → ESP32 3V3
POT-FC borne 2 → ESP32 GND
POT-FC curseur → ESP32 VN / GPIO39

AD8232 OUTPUT → ESP32 P34 / GPIO34
```

La condition d'urgence n°4 est :

```text
POT-FC anormal
      ET
AD8232 signal present
```

## 12. GPS NEO-6M-0-001

Pour le module nu, utiliser une alimentation conforme au module, typiquement 3,3 V :

```text
GPS VCC → ESP32 3V3
GPS GND → GND commun
GPS TX  → ESP32 P16 / GPIO16
GPS RX  ← ESP32 P17 / GPIO17
```

C'est un croisement UART : TX du GPS vers RX de l'ESP32 et RX du GPS depuis TX de l'ESP32.

## 13. GSM SIM800L

Le SIM800L utilise son alimentation dediee **4,00 V** issue du Buck 2 :

```text
Buck 2 OUT+ 4,00 V → SIM800L VCC/VBAT
Buck 2 OUT-        → SIM800L GND
```

Le buck doit supporter les pointes de courant du modem.

UART :

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

L'adaptation est utilisee pour ramener le niveau logique TX 3,3 V de l'ESP32 vers une tension adaptee a l'entree RX du SIM800L.

Connecter l'antenne GSM avant les essais d'emission.

## 14. Bouton SOS

```text
ESP32 P32 ───── bouton ───── GND
```

Le firmware utilise `INPUT_PULLUP` :

```text
Relache → HIGH
Appuye  → LOW
```

Le SOS est independant de la condition 5/5 et declenche une alerte immediate.

## 15. Buzzer

Pour un petit buzzer compatible GPIO :

```text
ESP32 P14 → Buzzer +
ESP32 GND → Buzzer -
```

Pour un buzzer demandant davantage de courant, utiliser un transistor de commande.

## 16. LED RGB

Pour une LED RGB a cathode commune :

```text
ESP32 P25 → resistance → R
ESP32 P33 → resistance → G
ESP32 P13 → resistance → B
Cathode commune → GND
```

Utiliser une resistance pour chaque couleur.

## 17. Regle d'urgence 5/5

```text
1. MPU6050 anormal / chute
          AND
2. POT-TMP hors plage
          AND
3. POT-PRESSION hors plage
          AND
4. POT-FC hors plage + AD8232 actif
          AND
5. MAX30102 anormal
          ↓
       5 / 5
          ↓
 confirmation 5 secondes
          ↓
       SMS GSM
```

Le GPS ajoute la position au SMS mais n'est pas une condition medicale du 5/5.

## 18. Ordre de validation

1. Regler **Buck 5 V à 5,00 V** au multimetre, sans ESP32 connecte.
2. Regler **Buck 4 V à 4,00 V** au multimetre, sans SIM800L connecte.
3. Verifier les polarites et la masse commune.
4. ESP32 + OLED.
5. Bus I²C + MPU6050.
6. MAX30102.
7. POT-TMP.
8. POT-PRESSION.
9. AD8232.
10. POT-FC.
11. GPS.
12. RGB + buzzer + SOS.
13. SIM800L + antenne + SIM.
14. Dashboard.
15. Test des 5 conditions d'urgence.

## 19. Securite

- Ne pas utiliser MediWatch pour un diagnostic ou une decision medicale.
- Ne pas connecter directement la Li-Po 3S aux modules basse tension.
- Ne jamais envoyer 5 V sur la broche 3V3 de l'ESP32.
- Ne jamais envoyer 5 V sur le SIM800L.
- Regler chaque buck au multimetre avant de connecter la charge.
- Ne jamais utiliser le connecteur d'equilibrage de la Li-Po comme alimentation improvisee.
- Utiliser un chargeur Li-Po 3S equilibre adapte.
