# Architecture MediWatch V2

## Vue d'ensemble

```text
                         MEDIWATCH V2

 TMP117 ───────┐
 MPU6050 ──────┤
 MAX30102 ─────┤
 OLED SH1106 ──┤
 AD8232 ───────┤
 Potentiomètre ┤
 GPS ──────────┤
 GSM ──────────┤
 SOS ──────────┤
 RGB/Buzzer ───┤
               ▼
             ESP32
        ┌──────┼──────────┐
        │      │          │
        ▼      ▼          ▼
      OLED    GSM       Wi-Fi AP
               │          │
               ▼          ▼
              SMS     Dashboard
               │
       ┌───────┴────────┐
       ▼                ▼
    Proches          Hôpital
```

## Couches

### 1. Acquisition

L'ESP32 lit :

- température via TMP117 ;
- accélération via MPU6050 ;
- signal optique via MAX30102 ;
- ECG analogique via AD8232 ;
- potentiomètre pour la pression simulée ;
- trames NMEA du GPS.

### 2. Traitement

Le firmware calcule ou maintient :

- température ;
- FC expérimentale ;
- SpO₂ expérimentale ;
- valeur ECG instantanée ;
- pression systolique/diastolique simulée ;
- position GPS ;
- magnitude de l'accélération ;
- état global.

### 3. Décision

```text
NORMAL
   │
   ├── anomalie non critique ───────> WARNING
   │
   ├── pression simulée critique
   │      └── confirmation 5 s ─────> CRITICAL
   │
   ├── chute expérimentale ─────────> CRITICAL
   │
   └── bouton SOS ──────────────────> SOS
```

### 4. Sorties

**Locale :**

- OLED ;
- LED RGB ;
- buzzer.

**Communication :**

- GSM → SMS ;
- GPS → coordonnées dans l'alerte ;
- Wi-Fi → dashboard local.

## Pourquoi Wi-Fi + GSM ?

Les deux communications ont des rôles différents :

- **GSM** : canal d'urgence indépendant du Wi-Fi local pour envoyer un SMS.
- **Wi-Fi** : interface de supervision locale riche pour afficher les mesures et l'état du prototype.

## Dashboard

Le serveur Web est directement exécuté par l'ESP32. Aucun serveur Node.js n'est requis pour le dashboard local V2.

```text
Téléphone / PC
      │
      │ Wi-Fi
      ▼
MEDIWATCH AP
      │
      ▼
ESP32 WebServer
      │
      ├── /
      ├── /api/data
      └── /api/status
```

## Philosophie de sécurité

Le prototype sépare explicitement les éléments simulés des éléments réellement acquis :

```text
PRESSION → SIMULÉE
GPS      → RÉEL
TEMP     → RÉEL / CAPTEUR
ECG      → ACQUISITION
FC       → EXPÉRIMENTALE
SpO₂     → EXPÉRIMENTALE
CHUTE    → EXPÉRIMENTALE
```

Aucune sortie du prototype ne doit être considérée comme une décision médicale.
