# MediWatch NEO

MediWatch NEO est la nouvelle architecture de simulation du bracelet MediWatch.

## Pourquoi NEO ?

Le TMP117 n'est pas disponible pour le prototype. Il est donc remplace par un potentiometre qui simule la temperature.

La pression arterielle reste simulee par un potentiometre.

L'AD8232 reste un vrai module : son signal analogique ECG est acquis et affiche. Comme une anomalie de frequence cardiaque ne peut pas etre provoquee volontairement de maniere fiable avec l'AD8232, un troisieme potentiometre simule la frequence cardiaque anormale.

## Sources

| Source | Role |
|---|---|
| Pot-TMP | temperature simulee |
| Pot-pression | pression simulee |
| AD8232 | ECG reel, acquisition uniquement |
| Pot-FC | simulation FC anormale |
| MAX30102 | FC + SpO2 experimentaux |
| MPU6050 | chute/mouvement experimental |
| GPS | localisation reelle |
| GSM | SMS |
| OLED | affichage local |

## Logique de selection de la FC

- Pot-FC entre 50 et 120 BPM : la source normale est le MAX30102 lorsque sa FC est valide.
- Pot-FC < 50 ou > 120 BPM : le Pot-FC devient la source effective de simulation d'anomalie.
- L'AD8232 continue dans tous les cas d'acquerir et d'afficher le signal ECG.

## Regle stricte d'envoi SMS

MediWatch NEO n'envoie **pas** de SMS automatique pour une anomalie isolée.

Les cinq conditions doivent etre vraies simultanement pendant 5 secondes :

```text
MPU anormal
   AND
Pot-TMP anormal
   AND
Pot-PRESSION anormal
   AND
Pot-FC anormal + AD8232 acquis
   AND
MAX30102 anormal
   = ALERTE GSM
```

Le bouton SOS est l'exception : il envoie immediatement.

## Pins

```text
I2C SDA       GPIO 21
I2C SCL       GPIO 22
AD8232 OUT    GPIO 34
Pot-TMP       GPIO 36
Pot-PRESSION  GPIO 35
Pot-FC        GPIO 39
GPS RX        GPIO 16
GPS TX        GPIO 17
GSM RX        GPIO 26
GSM TX        GPIO 27
SOS           GPIO 32
BUZZER        GPIO 14
RGB R         GPIO 25
RGB G         GPIO 33
RGB B         GPIO 13
```

## Dashboard

L'ESP32 cree un point d'acces Wi-Fi :

- SSID : `MEDIWATCH`
- Mot de passe : `mediwatch123`
- Adresse habituelle : `http://192.168.4.1`

Endpoints :

- `/` : dashboard
- `/api/data` : donnees du bracelet
- `/api/status` : statut des modules

## Limites

Ce projet est un prototype pedagogique. Temperature, pression et FC simulees ne sont pas des mesures medicales. Le signal ECG AD8232 n'est pas interprete. FC, SpO2 et detection de chute sont experimentaux. Ne pas utiliser pour un diagnostic ou une decision medicale.
