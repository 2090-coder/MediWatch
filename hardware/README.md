# Hardware MediWatch NEO

## Modules

- ESP32
- MAX30102 : frequence cardiaque + SpO2 experimental
- AD8232 : acquisition ECG reelle
- MPU6050 : mouvement / chute experimentale
- Potentiometre TEMP : simulation du TMP117 absent
- Potentiometre PRESSION : simulation de pression arterielle
- Potentiometre FC : simulation d'une anomalie de frequence cardiaque
- GPS UART : position reelle
- GSM UART : SMS d'urgence
- OLED SH1106 128x64 I2C
- Bouton SOS
- Buzzer
- LED RGB
- Resistances pour LED RGB

## Brochage NEO

| Fonction | ESP32 |
|---|---:|
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |
| AD8232 OUT | GPIO 34 |
| Potentiometre TEMP | GPIO 36 |
| Potentiometre PRESSION | GPIO 35 |
| Potentiometre FC | GPIO 39 |
| GPS RX | GPIO 16 |
| GPS TX | GPIO 17 |
| GSM RX | GPIO 26 |
| GSM TX | GPIO 27 |
| SOS | GPIO 32 |
| Buzzer | GPIO 14 |
| RGB Rouge | GPIO 25 |
| RGB Vert | GPIO 33 |
| RGB Bleu | GPIO 13 |

## Bus I2C

GPIO 21 (SDA) est partage par le MPU6050, MAX30102 et OLED.
GPIO 22 (SCL) est partage par le MPU6050, MAX30102 et OLED.

## Potentiometre TEMP — remplacement du TMP117

```text
3V3  -> extremite 1
GND  -> extremite 2
GPIO 36 -> curseur
```

Le firmware transforme la position en temperature simulee. Le TMP117 n'est pas necessaire dans NEO.

## Potentiometre PRESSION

```text
3V3  -> extremite 1
GND  -> extremite 2
GPIO 35 -> curseur
```

Il simule la pression systolique/diastolique. Ce n'est pas une mesure medicale.

## AD8232 + potentiometre FC

AD8232 reste branche normalement :

```text
AD8232 OUTPUT -> GPIO 34
AD8232 GND    -> GND
AD8232 3.3V   -> 3V3
```

Le signal ECG AD8232 est acquis et affiche.

Le potentiometre FC est independant :

```text
3V3  -> extremite 1
GND  -> extremite 2
GPIO 39 -> curseur
```

Il simule une frequence cardiaque. Si le potentiometre est dans la plage normale (50 a 120 BPM), la FC MAX30102 est conservee comme source effective lorsqu'elle est valide. Si le potentiometre sort de cette plage, il devient la source de simulation de l'anomalie.

## GPS

```text
GPS TX -> ESP32 GPIO 16 (RX)
GPS RX -> ESP32 GPIO 17 (TX)
GND commun
```

## GSM

```text
GSM TX -> ESP32 GPIO 26 (RX)
GSM RX -> ESP32 GPIO 27 (TX)
GND commun
```

Le GSM doit utiliser une alimentation adaptee a son modele et a ses pointes de courant. Ne pas l'alimenter depuis un GPIO de l'ESP32.

## SOS / buzzer / RGB

```text
SOS    : GPIO 32 -> bouton -> GND
Buzzer : GPIO 14
RGB R  : GPIO 25 + resistance
RGB G  : GPIO 33 + resistance
RGB B  : GPIO 13 + resistance
```

## Regle d'alerte MEDIWATCH NEO

L'affichage et le diagnostic de prototype restent actifs meme si une seule source est anormale.

**SMS automatique = uniquement si 5/5 conditions sont anormales simultanement :**

1. MPU6050 anormal / chute detectee
2. Pot-TMP hors plage normale
3. Pot-pression hors plage normale
4. Pot-FC hors plage normale, avec AD8232 reel acquis et affiche
5. MAX30102 anormal (FC ou SpO2 critique)

Le bouton **SOS** est volontairement une exception : il declenche un SMS immediat.

## Ordre de montage

1. ESP32 + alimentation
2. OLED + bus I2C
3. MPU6050
4. MAX30102
5. Pot-TMP
6. Pot-pression
7. AD8232
8. Pot-FC
9. GPS
10. RGB + buzzer + SOS
11. GSM et son alimentation adaptee

Tester chaque bloc avant d'ajouter le suivant.
