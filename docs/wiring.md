# MediWatch V2 — Câblage complet

## Vue générale

```text
                         ┌──────────────────┐
                         │       ESP32       │
                         └────────┬─────────┘
                                  │
          ┌───────────────────────┼──────────────────────┐
          │                       │                      │
         I²C                     UART                  ADC/GPIO
          │                       │                      │
  ┌───────┼────────┐       ┌─────┴─────┐        ┌──────┴──────┐
  │       │        │       │           │        │             │
 TMP117 MPU6050 MAX30102  GPS         GSM    AD8232         POT
  │       │        │       │           │        │             │
  └───────┴────────┴──┐    │           │        │             │
                      │    │           │        │             │
                    OLED   │           │        │             │
                      │    │           │        │             │
                      └────┴───────────┴────────┴─────────────┘

GPIO 32 → SOS
GPIO 14 → Buzzer
GPIO 25 → RGB Rouge
GPIO 33 → RGB Vert
GPIO 13 → RGB Bleu
```

## Tableau de référence

| Fonction | Module | ESP32 |
|---|---|---:|
| SDA | TMP117 / MPU6050 / MAX30102 / OLED | GPIO 21 |
| SCL | TMP117 / MPU6050 / MAX30102 / OLED | GPIO 22 |
| ECG analogique | AD8232 OUT | GPIO 34 |
| Pression simulée | Potentiomètre curseur | GPIO 35 |
| RX GPS | GPS TX | GPIO 16 |
| TX GPS | GPS RX | GPIO 17 |
| RX GSM | GSM TX | GPIO 26 |
| TX GSM | GSM RX | GPIO 27 |
| SOS | bouton vers GND | GPIO 32 |
| Buzzer | entrée commande | GPIO 14 |
| Rouge | LED RGB via résistance | GPIO 25 |
| Vert | LED RGB via résistance | GPIO 33 |
| Bleu | LED RGB via résistance | GPIO 13 |

## I²C partagé

```text
GPIO 21 SDA ──┬── TMP117 SDA
              ├── MPU6050 SDA
              ├── MAX30102 SDA
              └── OLED SDA

GPIO 22 SCL ──┬── TMP117 SCL
              ├── MPU6050 SCL
              ├── MAX30102 SCL
              └── OLED SCL
```

Partager SDA/SCL est normal pour des périphériques I²C ayant des adresses distinctes.

## Alimentation

Ne pas supposer que tous les breakouts ont les mêmes exigences. Vérifier le modèle exact avant de connecter VCC.

Le **GSM est le point critique** : son alimentation doit supporter les pointes de courant pendant l'émission. Ne pas alimenter un modem GSM directement depuis le 3,3 V de l'ESP32 sans validation du modèle.

Toutes les masses doivent être correctement référencées ensemble lorsque les modules communiquent avec l'ESP32.

## GPS

```text
GPS TX → ESP32 GPIO 16
GPS RX ← ESP32 GPIO 17
GPS GND → GND commun
GPS VCC → alimentation conforme au module
```

## GSM

```text
GSM TX → ESP32 GPIO 26
GSM RX ← ESP32 GPIO 27
GSM GND → GND commun
GSM VCC → alimentation conforme au module
```

## AD8232

```text
AD8232 OUTPUT → GPIO 34
AD8232 GND    → GND
AD8232 3.3V   → 3,3 V
```

RA, LA et RL sont raccordés aux électrodes selon le repérage du module.

## Potentiomètre

```text
3V3 ─────── extrémité
GPIO 35 ─── curseur
GND ─────── extrémité
```

La valeur produite est transformée en pression **simulée** par le firmware.

## SOS

```text
GPIO 32 ───── bouton ───── GND
```

Le firmware utilise `INPUT_PULLUP` : appui = LOW.

## Buzzer

Petit buzzer compatible GPIO :

```text
GPIO 14 → Buzzer +
GND     → Buzzer -
```

Pour un buzzer plus puissant, utiliser un transistor de commande et une alimentation adaptée.

## LED RGB

Pour une LED RGB à cathode commune :

```text
GPIO 25 ── résistance ── R
GPIO 33 ── résistance ── G
GPIO 13 ── résistance ── B
Cathode commune ─────── GND
```

Utiliser une résistance par couleur. Si la LED est à anode commune, inverser la logique de commande dans le firmware.

## Ordre de validation

1. ESP32 + alimentation
2. OLED
3. Bus I²C
4. TMP117
5. MPU6050
6. MAX30102
7. potentiomètre
8. AD8232
9. GPS
10. RGB + buzzer + SOS
11. GSM
12. dashboard et alertes

## Sécurité du prototype

- Ne pas utiliser les valeurs de MediWatch pour prendre une décision médicale.
- Ne pas connecter une personne à un montage dont l'alimentation ou l'isolation n'a pas été validée.
- L'AD8232 doit être utilisé conformément à la documentation du module et dans un contexte expérimental approprié.
- Le GSM doit être alimenté séparément/avec une alimentation dimensionnée lorsque nécessaire.
