# Téléversement MediWatch V1.2

## Fichier à utiliser

Pour le premier montage réel, utiliser :

`firmware/MediWatch_V1/MediWatch_V1_DIRECT.ino`

Ce fichier est autonome côté firmware : le dashboard Web est intégré dans le programme ESP32. Node.js n'est pas nécessaire.

## Bibliothèques Arduino

Installer dans le gestionnaire de bibliothèques :

1. U8g2
2. Adafruit TMP117
3. Adafruit MPU6050
4. Adafruit Unified Sensor
5. SparkFun MAX3010x Sensor Library
6. TinyGPSPlus

Les bibliothèques `Wire`, `WiFi` et `WebServer` viennent du core ESP32.

## Configuration avant téléversement

Modifier obligatoirement les trois contacts :

```cpp
String c1 = "+243XXXXXXXXX";
String c2 = "+243XXXXXXXXX";
String c3 = "+243XXXXXXXXX";
```

Modifier aussi le nom du patient si nécessaire.

## Connexion Web

Après téléversement :

1. Ouvrir le moniteur série à `115200` bauds.
2. Mettre sous tension l'ESP32.
3. Chercher le réseau Wi-Fi `MEDIWATCH`.
4. Mot de passe : `mediwatch123`.
5. Ouvrir l'adresse affichée par le moniteur série, normalement `http://192.168.4.1`.

## API

- `/` : dashboard
- `/api/data` : données JSON
- `/api/status` : état des modules

## Ordre de test recommandé

Ne pas brancher tous les modules simultanément au premier essai.

1. ESP32 + OLED
2. TMP117
3. MPU6050
4. MAX30102
5. potentiomètre
6. AD8232
7. GPS
8. GSM avec alimentation dédiée
9. SOS + buzzer + LED RGB
10. intégration complète

## Alimentation GSM

Le module GSM doit avoir une alimentation adaptée à son modèle et à ses pointes de courant. Ne pas l'alimenter depuis une simple sortie 3,3 V de l'ESP32.

## Limites V1.2

- La pression affichée est une simulation par potentiomètre.
- L'ECG est seulement échantillonné et affiché.
- Le calcul SpO2 est expérimental.
- La détection de chute est expérimentale.
- Les SMS sont bloquants pendant leur transmission.
- Cette version ne doit pas être utilisée comme dispositif médical.
