# Hardware MediWatch V2

## 1. Liste du matériel

- ESP32
- TMP117
- MPU6050
- MAX30102
- OLED SH1106 1,3" 128×64 I²C
- AD8232
- Potentiomètre
- GPS UART
- Module GSM UART
- Bouton SOS
- Buzzer
- LED RGB
- Résistances pour LED RGB
- Alimentation adaptée au GSM
- Fils / breadboard pour le prototype

## 2. Bus I²C partagé

Les quatre périphériques I²C utilisent le même bus :

```text
ESP32 GPIO 21 (SDA) ── TMP117 SDA
                    ├─ MPU6050 SDA
                    ├─ MAX30102 SDA
                    └─ OLED SDA

ESP32 GPIO 22 (SCL) ── TMP117 SCL
                    ├─ MPU6050 SCL
                    ├─ MAX30102 SCL
                    └─ OLED SCL
```

Alimentation et masse doivent être compatibles avec chaque breakout. Vérifier la documentation du module utilisé avant de brancher VCC.

## 3. Câblage détaillé

### TMP117

| TMP117 | ESP32 |
|---|---:|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| GND | GND |
| VCC | 3,3 V si le breakout l'exige |

### MPU6050

| MPU6050 | ESP32 |
|---|---:|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| GND | GND |
| VCC | alimentation compatible |

### MAX30102

| MAX30102 | ESP32 |
|---|---:|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| GND | GND |
| VCC | alimentation compatible avec le breakout |

### OLED SH1106 1,3"

| OLED | ESP32 |
|---|---:|
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| GND | GND |
| VCC | 3,3 V si compatible |

### AD8232

| AD8232 | ESP32 |
|---|---:|
| OUTPUT | GPIO 34 |
| GND | GND |
| 3.3V | 3,3 V |

Les électrodes RA, LA et RL sont raccordées au module AD8232 selon le repérage du breakout.

### Potentiomètre — pression simulée

```text
3V3 ───────── extrémité 1
GPIO 35 ───── curseur
GND ───────── extrémité 2
```

Le potentiomètre ne mesure **pas** la tension artérielle. Il sert uniquement à produire une valeur de démonstration transformée par le firmware en pression systolique/diastolique simulée.

### GPS

```text
GPS TX ───────> ESP32 GPIO 16 (RX)
GPS RX <────── ESP32 GPIO 17 (TX)
GPS GND ────── GND commun
GPS VCC ────── alimentation adaptée au module
```

### GSM

```text
GSM TX ───────> ESP32 GPIO 26 (RX)
GSM RX <────── ESP32 GPIO 27 (TX)
GSM GND ────── GND commun
GSM VCC ────── alimentation adaptée au modèle
```

**Point critique :** ne pas alimenter un modem GSM directement depuis le 3,3 V de l'ESP32 sans vérifier explicitement que le module le permet. Les transmissions GSM peuvent provoquer des pointes de courant importantes.

### Bouton SOS

```text
GPIO 32 ───── bouton ───── GND
```

Le firmware utilise `INPUT_PULLUP` :

- bouton relâché → HIGH
- bouton appuyé → LOW

### Buzzer

Pour un petit buzzer compatible GPIO :

```text
GPIO 14 ───── Buzzer +
GND ───────── Buzzer -
```

Si le buzzer demande plus de courant que le GPIO ne peut fournir, utiliser un transistor de commande et une alimentation adaptée.

### LED RGB à cathode commune

Utiliser une résistance série pour chaque couleur :

```text
GPIO 25 ── résistance ── Rouge
GPIO 33 ── résistance ── Vert
GPIO 13 ── résistance ── Bleu
Cathode commune ─────── GND
```

Si la LED est à **anode commune**, la logique électrique doit être inversée et le firmware devra être adapté.

## 4. Tableau GPIO complet

| Fonction | GPIO ESP32 |
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

## 5. Masse commune

Tous les modules qui communiquent directement avec l'ESP32 doivent partager une référence GND appropriée :

```text
                 ┌── TMP117
                 ├── MPU6050
ESP32 GND ───────┼── MAX30102
                 ├── OLED
                 ├── AD8232
                 ├── GPS
                 ├── GSM
                 ├── Buzzer
                 └── RGB / bouton
```

Pour le GSM, la masse de son alimentation doit également être référencée correctement à la masse de l'ESP32 pour que l'UART fonctionne.

## 6. Alimentation

Ne pas supposer que tous les modules acceptent la même tension. Vérifier le modèle exact du breakout.

Le GSM est le point le plus sensible : prévoir une alimentation capable de fournir les pointes de courant du modem et des condensateurs de découplage appropriés selon le module.

## 7. Ordre de montage recommandé

1. ESP32 + alimentation stable
2. OLED
3. TMP117
4. MPU6050
5. MAX30102
6. Potentiomètre
7. AD8232
8. GPS
9. RGB + buzzer + SOS
10. GSM avec son alimentation dédiée/appropriée

Tester chaque bloc avant d'ajouter le suivant.

## Statut

**MediWatch V2 — câblage de référence pour le prototype.**

Le brochage décrit ici correspond au firmware `firmware/MediWatch_V2/MediWatch_V2.ino`.
