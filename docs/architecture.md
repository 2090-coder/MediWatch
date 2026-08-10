# Architecture MediWatch V1

## Vue d'ensemble

```text
                 MEDIWATCH V1

  TMP117 ───────┐
  MAX30102 ────┤
  AD8232 ──────┤
  MPU6050 ─────┤
  Potentiomètre ┤
  GPS ─────────┤
  SOS ─────────┤
                ↓
             ESP32
       ┌────────┼─────────┐
       ↓        ↓         ↓
     OLED     GSM       Wi-Fi
       │        │         │
       │       SMS     Dashboard
       │      alerte      Web
       ↓
    Patient
```

## Philosophie

L'ESP32 reste le contrôleur principal. Aucun serveur Node.js n'est nécessaire pour la V1 : le serveur HTTP est directement exécuté sur l'ESP32.

Le GSM est réservé aux SMS d'urgence. Le Wi-Fi sert à l'interface locale de supervision.

## États

- `NORMAL` : paramètres dans les limites définies.
- `WARNING` : paramètre anormal mais non critique.
- `CRITICAL` : paramètre critique ou chute détectée.
- `SOS` : déclenchement manuel du bouton d'urgence.

## Sécurité de conception

Les mesures sont destinées au prototype. Toute décision médicale doit être prise avec des instruments et procédures médicales appropriés.
