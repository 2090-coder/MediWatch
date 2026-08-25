# MediWatch NEO — Câblage d'alimentation

## 1. Batterie utilisée

Batterie : **Li-Po 3S 11,1 V — 1100 mAh**.

Une batterie 3S possède :

```text
Tension nominale : 11,1 V
Tension maximale chargée : 12,6 V
Tension minimale pratique : à surveiller selon la batterie et le système de protection
```

**La batterie ne doit jamais être branchée directement sur l'ESP32, le SIM800L ou les modules 3,3 V.**

Le connecteur blanc à 4 broches visible sur la batterie est le **connecteur d'équilibrage 3S**. Il est destiné au chargeur équilibré et à la surveillance des cellules. Il ne sert pas à alimenter le MediWatch pendant le fonctionnement normal.

Le gros connecteur rouge/noir est le connecteur principal de puissance de la batterie.

## 2. Architecture d'alimentation définitive

Le MediWatch NEO utilise deux rails principaux issus de la Li-Po :

```text
                    Li-Po 3S 11,1 V
                    12,6 V maximum
                           │
                     CONNECTEUR +
                           │
                         FUSIBLE
                           │
                    INTERRUPTEUR ON/OFF
                           │
                 +---------+---------+
                 │                   │
                 ▼                   ▼
          BUCK N°1  → 5,00 V   BUCK N°2  → 4,00 V
                 │                   │
                 │                   └────→ SIM800L VCC/VBAT
                 │
                 └────→ ESP32 VIN/5V

Li-Po GND ────────────────┬───────────────┬──────────────
                          │               │
                       Buck 1          Buck 2
                          │               │
                          └───────┬───────┘
                                  │
                              GND COMMUN
                                  │
                 +----------------+----------------+
                 │                │                │
               ESP32            GPS        autres modules
```

## 3. Buck N°1 — rail 5,00 V

Le premier convertisseur abaisse la tension variable de la batterie (jusqu'à 12,6 V) vers **5,00 V**.

Avant de connecter l'ESP32 :

1. Débrancher la charge du buck.
2. Brancher uniquement l'entrée du buck à la batterie via le fusible et l'interrupteur.
3. Mesurer la sortie au multimètre.
4. Régler le potentiomètre du buck à **5,00 V**.
5. Vérifier que la tension reste stable.
6. Ensuite seulement connecter la sortie 5 V à l'entrée **VIN/5V** de la carte ESP32 NodeMCU-32S.

```text
Li-Po + → fusible → interrupteur → Buck 1 IN+
Li-Po - ─────────────────────────→ Buck 1 IN-

Buck 1 OUT+ = 5,00 V → ESP32 VIN/5V
Buck 1 OUT-          → ESP32 GND
```

**Ne jamais connecter les 5 V du buck sur la broche 3V3 de l'ESP32.**

## 4. Buck N°2 — alimentation dédiée SIM800L

Le SIM800L est alimenté par un rail séparé réglé à **4,00 V**.

```text
Li-Po + → fusible → interrupteur → Buck 2 IN+
Li-Po - ─────────────────────────→ Buck 2 IN-

Buck 2 OUT+ = 4,00 V → SIM800L VCC/VBAT
Buck 2 OUT-          → SIM800L GND
```

Le buck du SIM800L doit être capable de fournir les **pointes de courant du modem**, idéalement au moins **2 A en pointe** avec une alimentation réellement stable.

**Ne jamais alimenter le SIM800L avec 5 V.**

Ajouter un condensateur de réservoir/decouplage au plus près du SIM800L, conformément au module utilisé, afin de limiter les chutes de tension pendant l'émission GSM.

## 5. Masse commune

La masse est commune entre les deux bucks et l'ESP32 :

```text
Li-Po GND
   │
   ├── Buck 1 IN-
   ├── Buck 2 IN-
   │
   └── GND commun système
          ├── ESP32 GND
          ├── OLED GND
          ├── MPU6050 GND
          ├── MAX30102 GND
          ├── AD8232 GND
          ├── GPS GND
          ├── SIM800L GND
          ├── POT-TMP GND
          ├── POT-PRESSION GND
          ├── POT-FC GND
          ├── Buzzer GND
          └── RGB GND/cathode commune
```

Cette masse commune est nécessaire pour que les signaux UART, I2C et analogiques aient la même référence électrique.

## 6. Rail 3,3 V

Les modules basse tension du prototype utilisent le **3V3 de l'ESP32 lorsque leur breakout est explicitement compatible 3,3 V** :

```text
ESP32 3V3
   ├── OLED SH1106 VCC
   ├── MPU6050 VCC
   ├── MAX30102 VCC
   ├── AD8232 3.3V
   ├── POT-TMP extrémité 1
   ├── POT-PRESSION extrémité 1
   └── POT-FC extrémité 1
```

Le courant disponible sur le 3V3 de la carte doit rester dans les limites de la carte et des modules. Ne pas utiliser cette sortie pour le SIM800L.

## 7. GPS NEO-6M-0-001

Pour le **module u-blox NEO-6M-0-001 nu**, utiliser une alimentation conforme à sa tension d'entrée, typiquement **3,3 V** :

```text
ESP32 3V3 → GPS VCC
GND commun → GPS GND
GPS TX → ESP32 P16 / GPIO16
GPS RX ← ESP32 P17 / GPIO17
```

Si le GPS est un breakout avec régulateur, vérifier le marquage exact de la carte avant d'appliquer une autre tension.

## 8. Potentiomètres

Chaque potentiomètre de simulation est alimenté entre 3,3 V et GND afin que son curseur reste dans la plage ADC de l'ESP32 :

```text
3V3 ───── borne 1
          POT
GPIO ADC ─ curseur
GND ───── borne 2
```

Affectations :

```text
POT-TMP       curseur → VP / GPIO36
POT-PRESSION  curseur → P35 / GPIO35
POT-FC        curseur → VN / GPIO39
```

**Ne pas alimenter les potentiomètres avec 5 V** lorsque leur curseur est relié directement à l'ADC de l'ESP32.

## 9. AD8232

```text
AD8232 3.3V   → ESP32 3V3
AD8232 GND    → GND commun
AD8232 OUTPUT → ESP32 P34 / GPIO34
```

Le signal ECG est acquis expérimentalement. Il n'est pas interprété médicalement.

## 10. Protection et ordre de branchement

Ordre recommandé côté puissance :

```text
Li-Po +
  │
  ├── fusible adapté
  │
  └── interrupteur général
          │
          ├── Buck 1 → 5,00 V → ESP32 VIN/5V
          │
          └── Buck 2 → 4,00 V → SIM800L

Li-Po - ─────────────────────────────→ masse commune
```

Le fusible doit être placé **près de la batterie**, sur le conducteur positif.

## 11. Connecteur d'équilibrage de la batterie

La prise blanche 4 broches de la Li-Po 3S est le câble d'équilibrage :

```text
Cellule 1 : B-
Cellule 1/2 : point intermédiaire
Cellule 2/3 : point intermédiaire
Cellule 3 : B+
```

Ne pas utiliser ces fils comme sorties d'alimentation séparées pour les modules. Ils doivent rester destinés à la fonction d'équilibrage/charge et être raccordés uniquement à un chargeur Li-Po 3S équilibré ou à un système de gestion explicitement prévu pour une batterie 3S.

## 12. Vérification obligatoire avant première mise sous tension

Avec la batterie branchée et les sorties des bucks encore déconnectées des modules :

```text
Buck 1 : mesurer OUT+ ↔ OUT- → 5,00 V
Buck 2 : mesurer OUT+ ↔ OUT- → 4,00 V
```

Puis vérifier :

```text
5 V → ESP32 VIN/5V       ✓
4 V → SIM800L            ✓
3,3 V → modules 3,3 V    ✓
5 V → ESP32 3V3          ✗ INTERDIT
5 V → SIM800L            ✗ INTERDIT
12,6 V → ESP32           ✗ INTERDIT
12,6 V → SIM800L         ✗ INTERDIT
```

## 13. Schéma final compact

```text
                 MEDIWATCH NEO

              Li-Po 3S 11,1 V
                12,6 V MAX
                    │
             +──────┴──────+
             │  +          │
             │ FUSIBLE     │
             │  +          │
             │ INTERRUPTEUR│
             +──────┬──────+
                    │
             +------+------+
             │             │
          BUCK 5V       BUCK 4V
             │             │
          5,00 V          4,00 V
             │             │
          ESP32          SIM800L
          VIN/5V          VCC
             │             │
             +──────┬──────+
                    │
                 GND COMMUN
                    │
          +---------+---------+
          │         │         │
        3V3       UART       I2C
          │         │         │
     GPS/ADC/     GPS/GSM   OLED/MPU/
      capteurs                MAX
```

## 14. Règles de sécurité

- Une Li-Po 3S chargée atteint **12,6 V** : dimensionner tous les composants d'entrée en conséquence.
- Ne jamais court-circuiter la batterie.
- Ne jamais brancher directement la Li-Po sur l'ESP32 ou le SIM800L.
- Régler les bucks au multimètre avant de connecter les charges.
- Respecter la polarité `+` et `-`.
- Ne jamais utiliser le connecteur d'équilibrage comme sortie improvisée.
- Utiliser un chargeur Li-Po 3S équilibré adapté à la batterie.
- Pour les essais, placer le système sur une surface non inflammable et surveiller la batterie et les convertisseurs.
- Si la batterie gonfle, chauffe anormalement ou est endommagée, interrompre immédiatement son utilisation.

## 15. Statut

Cette architecture d'alimentation correspond au prototype **MediWatch NEO** avec :

- Li-Po 3S 11,1 V / 1100 mAh
- Buck 5,00 V pour l'ESP32
- Buck 4,00 V dédié au SIM800L
- rail 3,3 V pour les modules compatibles
- masse commune
- fusible + interrupteur général
