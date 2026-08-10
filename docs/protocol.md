# Protocole Web MediWatch V1

Le dashboard communique directement avec le serveur HTTP de l'ESP32.

## GET /api/data

Retourne un objet JSON contenant notamment :

```json
{
  "patient": "UWASE",
  "heartRate": 72,
  "spo2": 98,
  "temperature": 36.7,
  "systolic": 120,
  "diastolic": 80,
  "latitude": -1.68,
  "longitude": 29.23,
  "gpsValid": true,
  "ecg": 2048,
  "fallDetected": false,
  "state": "NORMAL"
}
```

Les noms définitifs doivent rester cohérents entre le firmware et le JavaScript du dashboard.

## Rafraîchissement

La V1 peut utiliser des requêtes HTTP périodiques depuis le navigateur. Une version ultérieure pourra utiliser WebSocket ou Server-Sent Events si la fréquence de données ECG l'exige.
