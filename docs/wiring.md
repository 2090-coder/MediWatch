# Câblage MediWatch V1

## I2C

- ESP32 GPIO 21 → SDA de TMP117, MPU6050, MAX30102 et OLED
- ESP32 GPIO 22 → SCL de TMP117, MPU6050, MAX30102 et OLED
- GND commun
- Alimentation selon les spécifications de chaque module

## Analogique

- AD8232 OUT → GPIO 34
- Potentiomètre sortie centrale → GPIO 35

## GPS

- GPS TX → ESP32 GPIO 16 (RX)
- GPS RX → ESP32 GPIO 17 (TX)
- GND commun

## GSM

- GSM TX → ESP32 GPIO 26 (RX)
- GSM RX → ESP32 GPIO 27 (TX)
- GND commun
- Utiliser une alimentation adaptée au module GSM et à ses pointes de courant.

## Urgence

- Bouton SOS entre GPIO 32 et GND (`INPUT_PULLUP`)
- Buzzer → GPIO 14
- LED rouge → GPIO 25 via résistance adaptée
- LED verte → GPIO 33 via résistance adaptée
- LED bleue → GPIO 13 via résistance adaptée

## Important

Ne pas alimenter directement un module GSM puissant depuis une sortie GPIO. Vérifier les niveaux logiques, la tension et le courant admissibles de chaque module avant le montage.
