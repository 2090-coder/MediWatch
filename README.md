# MediWatch

**MediWatch — Bracelet de surveillance connectée basé sur ESP32**

MediWatch est un **prototype expérimental** développé pour explorer la surveillance de paramètres physiologiques, la détection d'événements et l'envoi d'alertes avec un ESP32.

> **AVERTISSEMENT IMPORTANT** : MediWatch n'est pas un dispositif médical certifié. La pression artérielle est **simulée par potentiomètre** dans ce prototype. L'ECG n'est pas interprété médicalement, la FC/SpO₂ sont expérimentales et la détection de chute est expérimentale. Le système ne doit pas être utilisé pour diagnostiquer, traiter ou surveiller médicalement une personne.

## Objectif

Lorsqu'un événement critique est détecté, MediWatch doit pouvoir :

1. avertir localement avec LED RGB + buzzer ;
2. afficher l'état sur l'OLED 1,3" ;
3. envoyer une alerte GSM aux proches ;
4. envoyer l'alerte à un contact hospitalier configuré ;
5. joindre la position GPS sous forme de lien Google Maps ;
6. afficher les données en temps réel sur un dashboard Web local hébergé par l'ESP32.

## V2 — Architecture actuelle

```text
                         ┌──────────────────┐
                         │       ESP32       │
                         │  contrôleur V2    │
                         └────────┬─────────┘
                                  │
              ┌───────────────────┼───────────────────┐
              │                   │                   │
             I²C                 UART              Analogique
              │                   │                   │
       ┌──────┼───────┐      ┌────┴────┐        ┌─────┴─────┐
       │      │       │      │         │        │           │
     TMP117 MPU6050 MAX30102 GPS       GSM    AD8232      POT
       │      │       │      │         │        │           │
       └──────┴───────┴──┐   │         │        │           │
                         │   │         │        │           │
                      OLED  │         │        │           │
                         │   │         │        │           │
                         └───┴─────────┴────────┴───────────┘

                GPIO 32 → SOS
                GPIO 14 → Buzzer
                GPIO 25 → RGB Rouge
                GPIO 33 → RGB Vert
                GPIO 13 → RGB Bleu

                         ESP32 Wi-Fi
                              │
                              ▼
                       Dashboard local
```

## Modules V2

| Module | Rôle | Connexion |
|---|---|---|
| ESP32 | Contrôleur | — |
| TMP117 | Température | I²C |
| MPU6050 | Mouvement / chute expérimentale | I²C |
| MAX30102 | FC + SpO₂ expérimentale | I²C |
| OLED SH1106 1,3" | Affichage | I²C |
| AD8232 | Acquisition ECG | GPIO 34 |
| Potentiomètre | Simulation pression | GPIO 35 |
| GPS | Position réelle | UART1 |
| GSM | SMS réel | UART2 |
| Bouton SOS | Alerte manuelle | GPIO 32 |
| Buzzer | Alerte locale | GPIO 14 |
| LED RGB | État système | GPIO 25/33/13 |

## Brochage V2

| Fonction | ESP32 |
|---|---:|
| I²C SDA — TMP117 | GPIO 21 |
| I²C SCL — TMP117 | GPIO 22 |
| I²C SDA — MPU6050 | GPIO 21 |
| I²C SCL — MPU6050 | GPIO 22 |
| I²C SDA — MAX30102 | GPIO 21 |
| I²C SCL — MAX30102 | GPIO 22 |
| I²C SDA — OLED | GPIO 21 |
| I²C SCL — OLED | GPIO 22 |
| AD8232 OUT | GPIO 34 |
| Potentiomètre OUT | GPIO 35 |
| GPS TX → ESP32 RX | GPIO 16 |
| GPS RX ← ESP32 TX | GPIO 17 |
| GSM TX → ESP32 RX | GPIO 26 |
| GSM RX ← ESP32 TX | GPIO 27 |
| Bouton SOS | GPIO 32 |
| Buzzer | GPIO 14 |
| RGB Rouge | GPIO 25 |
| RGB Vert | GPIO 33 |
| RGB Bleu | GPIO 13 |

Tous les modules doivent avoir une **masse commune**. Les tensions d'alimentation doivent être vérifiées selon le modèle exact de chaque module. Le GSM doit disposer d'une alimentation capable de fournir ses pointes de courant.

Voir [`hardware/README.md`](hardware/README.md) et [`docs/wiring.md`](docs/wiring.md).

## Logique d'alerte V2

| Condition | État | Réaction |
|---|---|---|
| Fonctionnement normal | NORMAL | LED verte |
| Pression simulée ≥ 140 mmHg | WARNING | LED jaune |
| Pression simulée ≥ 160 mmHg pendant 5 s | CRITICAL | LED rouge + buzzer + SMS |
| Température ≥ 38 °C | WARNING | LED jaune |
| Température ≥ 39 °C | CRITICAL | LED rouge + buzzer + SMS |
| SpO₂ ≤ 94 % | WARNING | LED jaune |
| SpO₂ ≤ 90 % | CRITICAL | LED rouge + buzzer + SMS |
| FC < 50 ou > 120 BPM | WARNING | LED jaune |
| Chute expérimentale | CRITICAL | Alerte immédiate |
| SOS | SOS | Alerte immédiate |

Le SOS et une chute critique ne doivent pas attendre la confirmation de 5 secondes utilisée pour la pression simulée.

## Alertes GSM

Le message d'urgence peut être envoyé à :

- `contact1`
- `contact2`
- `contact3`
- `hospitalContact`

Le SMS contient notamment le patient, le motif, les valeurs disponibles et un lien Google Maps lorsque le GPS est valide.

Avant tout essai, remplacer les numéros `+243XXXXXXXXX` dans le firmware V2.

## Dashboard Web

L'ESP32 crée un point d'accès Wi-Fi local :

- **SSID :** `MEDIWATCH`
- **Mot de passe :** `mediwatch123`
- **Adresse :** `http://192.168.4.1`

Le dashboard affiche :

- fréquence cardiaque ;
- SpO₂ ;
- température ;
- pression simulée ;
- ECG acquis ;
- latitude / longitude ;
- lien Google Maps ;
- état du système ;
- motif de l'alerte ;
- détection de chute.

API principales :

- `GET /` — dashboard
- `GET /api/data` — données du bracelet en JSON
- `GET /api/status` — état des modules

## Arborescence

```text
MediWatch/
├── firmware/
│   ├── MediWatch_V1/
│   │   └── MediWatch_V1.ino
│   └── MediWatch_V2/
│       ├── MediWatch_V2.ino
│       └── README.md
├── hardware/
│   └── README.md
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   └── wiring.md
├── web/
├── .gitignore
├── LICENSE
└── README.md
```

## Bibliothèques Arduino V2

Installer :

- Adafruit TMP117
- Adafruit MPU6050
- Adafruit Unified Sensor
- SparkFun MAX3010x Sensor Library
- TinyGPSPlus
- U8g2

`Wire`, `WiFi` et `WebServer` sont fournis par le core ESP32.

## Mise en route

1. Installer Arduino IDE.
2. Installer le core ESP32.
3. Installer les bibliothèques V2.
4. Ouvrir `firmware/MediWatch_V2/MediWatch_V2.ino`.
5. Sélectionner la carte ESP32 correspondant au matériel utilisé.
6. Réaliser le câblage décrit dans `docs/wiring.md`.
7. Vérifier particulièrement l'alimentation du GSM.
8. Configurer les quatre numéros GSM dans le firmware.
9. Téléverser le programme.
10. Ouvrir le moniteur série à 115200 bauds.
11. Connecter un téléphone ou PC au Wi-Fi `MEDIWATCH`.
12. Ouvrir `http://192.168.4.1`.

## Ordre de test recommandé

```text
1. ESP32 + OLED
2. Bus I²C
3. TMP117
4. MPU6050
5. MAX30102
6. AD8232
7. Potentiomètre
8. GPS
9. LED + buzzer + SOS
10. GSM
11. Dashboard
12. Test d'alerte complet
```

Ne branchez pas tous les modules simultanément lors du premier test sans avoir validé leurs alimentations.

## Limites techniques

- La pression est une **simulation**, pas une mesure.
- Le SpO₂ n'est pas une mesure médicale certifiée.
- La fréquence cardiaque dépend de la qualité du signal du MAX30102.
- L'ECG est seulement acquis et visualisé ; aucune interprétation clinique n'est effectuée.
- La détection de chute est expérimentale.
- Le GPS peut être indisponible à l'intérieur.
- Le GSM dépend du réseau, de la SIM et de l'alimentation du modem.
- Le dashboard V2 est local au Wi-Fi créé par l'ESP32 ; ce n'est pas encore une plateforme Internet distante.

## Statut

**MediWatch V2 — Prototype fonctionnel en développement.**

La V2 constitue la base de travail pour les essais matériels, l'amélioration de la fiabilité des mesures et les futures versions du bracelet.
