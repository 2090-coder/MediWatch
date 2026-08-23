/*
 * ============================================================
 *                      MEDIWATCH NEO V2
 * ============================================================
 *
 * Carte :
 * ESP32 NodeMCU-32S
 *
 * ------------------------------------------------------------
 * CAPTEURS / MODULES
 * ------------------------------------------------------------
 *
 * POT-TMP       -> GPIO36 / VP
 * POT-PRESSION  -> GPIO35
 * POT-FC        -> GPIO39 / VN
 *
 * AD8232 ECG    -> GPIO34
 *
 * MPU6050       -> SDA GPIO21 / SCL GPIO22
 * MAX30102      -> SDA GPIO21 / SCL GPIO22
 * OLED SH1106   -> SDA GPIO21 / SCL GPIO22
 *
 * GPS NEO-6M    -> RX GPIO16 / TX GPIO17
 * SIM800L       -> RX GPIO26 / TX GPIO27
 *
 * SOS           -> GPIO32
 * BUZZER        -> GPIO14
 *
 * RGB
 * R             -> GPIO25
 * G             -> GPIO33
 * B             -> GPIO13
 *
 * ============================================================
 *
 * LOGIQUE MEDIWATCH
 * ============================================================
 *
 * CONDITION 1 :
 * MPU6050 -> chute / anomalie mouvement
 *
 * CONDITION 2 :
 * POT-TMP -> température anormale
 *
 * CONDITION 3 :
 * POT-PRESSION -> pression anormale
 *
 * CONDITION 4 :
 * POT-FC anormale + signal AD8232 présent
 *
 * CONDITION 5 :
 * MAX30102 -> FC ou SpO2 anormale
 *
 * SMS AUTOMATIQUE :
 *
 * Les 5 conditions doivent être anormales
 * simultanément pendant ALERT_CONFIRM_TIME.
 *
 * SOS :
 *
 * Bouton SOS -> SMS immédiat.
 *
 * ============================================================
 *
 * IMPORTANT :
 *
 * Prototype pédagogique.
 *
 * La température et la pression sont simulées.
 * La FC peut être simulée par potentiomètre.
 * L'ECG AD8232 est acquis mais n'est pas interprété
 * médicalement.
 *
 * Ne pas utiliser pour un diagnostic médical.
 *
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

#include <U8g2lib.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#include <MAX30105.h>
#include "spo2_algorithm.h"

#include <TinyGPS++.h>

#include <math.h>


// ============================================================
// BROCHES
// ============================================================

#define PIN_SDA 21
#define PIN_SCL 22

#define PIN_ECG 34

#define PIN_TEMP_POT 36
#define PIN_PRESS_POT 35
#define PIN_HR_POT 39

#define PIN_GPS_RX 16
#define PIN_GPS_TX 17

#define PIN_GSM_RX 26
#define PIN_GSM_TX 27

#define PIN_SOS 32
#define PIN_BUZZER 14

#define PIN_LED_R 25
#define PIN_LED_G 33
#define PIN_LED_B 13


// ============================================================
// WIFI
// ============================================================

const char* WIFI_SSID = "MEDIWATCH";
const char* WIFI_PASSWORD = "mediwatch123";


// ============================================================
// PATIENT / CONTACTS
// ============================================================

String patientName = "UWASE";

String contact1 = "+243XXXXXXXXX";
String contact2 = "+243XXXXXXXXX";
String contact3 = "+243XXXXXXXXX";

String hospitalContact = "+243XXXXXXXXX";


// ============================================================
// SEUILS TEMPERATURE
// ============================================================

const float TEMP_MIN = 36.0;
const float TEMP_MAX = 37.9;


// ============================================================
// SEUILS PRESSION
// ============================================================

const float PRESS_MIN = 90.0;
const float PRESS_MAX = 139.0;


// ============================================================
// SEUILS FC
// ============================================================

const int HR_LOW = 50;
const int HR_HIGH = 120;


// ============================================================
// SEUIL SPO2
// ============================================================

const int SPO2_CRITICAL = 90;


// ============================================================
// MPU6050
// ============================================================

const float MPU_FREE_FALL = 2.5;
const float MPU_IMPACT = 22.0;

const unsigned long FALL_CONFIRM_TIME = 250;


// ============================================================
// TIMERS
// ============================================================

const unsigned long SENSOR_INTERVAL = 100;
const unsigned long DISPLAY_INTERVAL = 500;

const unsigned long ALERT_CONFIRM_TIME = 5000;

const unsigned long GPS_STALE_TIME = 5000;

const unsigned long SOS_COOLDOWN = 3000;

const unsigned long GSM_TIMEOUT = 5000;

const unsigned long ECG_WINDOW_TIME = 500;


// ============================================================
// OBJETS
// ============================================================

HardwareSerial GPSSerial(1);
HardwareSerial GSMSerial(2);

WebServer server(80);

TinyGPSPlus gps;

Adafruit_MPU6050 mpu;

MAX30105 max30102;

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,
  U8X8_PIN_NONE
);


// ============================================================
// ETATS MODULES
// ============================================================

bool mpuOK = false;
bool maxOK = false;
bool gsmOK = false;
bool gpsValid = false;


// ============================================================
// ETATS CONDITIONS
// ============================================================

bool tempAbnormal = false;
bool pressureAbnormal = false;

bool hrPotAbnormal = false;

bool ad8232Active = false;

bool maxAbnormal = false;

bool mpuAbnormal = false;

bool fallDetected = false;

bool allAbnormal = false;

bool emergencySent = false;


// ============================================================
// VALEURS
// ============================================================

float temperature = 36.5;

float systolic = 120.0;
float diastolic = 78.0;

int simulatedHR = 75;

int heartRate = 0;

int effectiveHR = 0;

int spo2 = 0;

int ecgValue = 0;

float latitude = 0.0;
float longitude = 0.0;

float accelX = 0.0;
float accelY = 0.0;
float accelZ = 0.0;

float accelMagnitude = 9.81;


// ============================================================
// VALIDATION MAX30102
// ============================================================

bool hrValid = false;
bool spo2Valid = false;

bool maxDataReady = false;

String hrSource = "AUCUNE";


// ============================================================
// ETAT SYSTEME
// ============================================================

String state = "NORMAL";

String reason = "Aucune anomalie";


// ============================================================
// TIMERS
// ============================================================

unsigned long lastSensorRead = 0;
unsigned long lastDisplay = 0;

unsigned long lastBeep = 0;

unsigned long allAbnormalSince = 0;

unsigned long lastSOS = 0;

unsigned long sosUntil = 0;

unsigned long lowAccelSince = 0;


// ============================================================
// ECG
// ============================================================

int ecgMin = 4095;
int ecgMax = 0;

unsigned long ecgWindowStart = 0;


// ============================================================
// MAX30102 BUFFER
// ============================================================

const int SPO2_SAMPLES = 100;

uint32_t irBuffer[SPO2_SAMPLES];
uint32_t redBuffer[SPO2_SAMPLES];

int sampleCount = 0;


// ============================================================
// RGB
// ============================================================

void setRGB(
  bool r,
  bool g,
  bool b
)
{
  digitalWrite(
    PIN_LED_R,
    r ? HIGH : LOW
  );

  digitalWrite(
    PIN_LED_G,
    g ? HIGH : LOW
  );

  digitalWrite(
    PIN_LED_B,
    b ? HIGH : LOW
  );
}


// ============================================================
// BUZZER
// ============================================================

void buzzerOff()
{
  digitalWrite(
    PIN_BUZZER,
    LOW
  );

  lastBeep = millis();
}


void buzzerUpdate()
{
  if (
    millis() - lastBeep >= 400
  )
  {
    lastBeep = millis();

    digitalWrite(
      PIN_BUZZER,
      !digitalRead(PIN_BUZZER)
    );
  }
}


void buzzerShort()
{
  digitalWrite(
    PIN_BUZZER,
    HIGH
  );
}


// ============================================================
// TEMPERATURE
// ============================================================

void readTemperature()
{
  int raw = analogRead(
    PIN_TEMP_POT
  );

  temperature =
      30.0 +
      (
        15.0 *
        raw /
        4095.0
      );

  tempAbnormal =
      (
        temperature < TEMP_MIN ||
        temperature > TEMP_MAX
      );
}


// ============================================================
// PRESSION
// ============================================================

void readPressure()
{
  int raw = analogRead(
    PIN_PRESS_POT
  );

  /*
   * Plage simulée :
   *
   * 70 -> 200 mmHg
   *
   * Cela permet notamment de simuler :
   *
   * 90
   * 120
   * 140
   * 160
   * 180
   * 200
   */

  systolic =
      70.0 +
      (
        130.0 *
        raw /
        4095.0
      );

  diastolic =
      systolic * 0.65;

  pressureAbnormal =
      (
        systolic < PRESS_MIN ||
        systolic > PRESS_MAX
      );
}


// ============================================================
// POTENTIOMETRE FC
// ============================================================

void readHeartRatePot()
{
  int raw = analogRead(
    PIN_HR_POT
  );

  simulatedHR =
      40 +
      (
        140 *
        raw /
        4095
      );

  hrPotAbnormal =
      (
        simulatedHR < HR_LOW ||
        simulatedHR > HR_HIGH
      );
}


// ============================================================
// AD8232
// ============================================================

void readECG()
{
  ecgValue =
      analogRead(PIN_ECG);

  if (
    ecgValue < ecgMin
  )
  {
    ecgMin = ecgValue;
  }

  if (
    ecgValue > ecgMax
  )
  {
    ecgMax = ecgValue;
  }


  if (
    millis() - ecgWindowStart >=
    ECG_WINDOW_TIME
  )
  {
    int amplitude =
        ecgMax - ecgMin;

    /*
     * On considère qu'un signal est présent
     * si le signal possède une variation
     * suffisante.
     */

    ad8232Active =
        (
          amplitude >= 25 &&
          ecgValue > 10 &&
          ecgValue < 4090
        );

    ecgMin = 4095;
    ecgMax = 0;

    ecgWindowStart = millis();
  }
}


// ============================================================
// MPU6050
// ============================================================

void readMPU()
{
  if (!mpuOK)
  {
    mpuAbnormal = false;
    return;
  }


  sensors_event_t accel;
  sensors_event_t gyro;
  sensors_event_t temp;


  mpu.getEvent(
    &accel,
    &gyro,
    &temp
  );


  accelX =
      accel.acceleration.x;

  accelY =
      accel.acceleration.y;

  accelZ =
      accel.acceleration.z;


  accelMagnitude =
      sqrt(
        accelX * accelX +
        accelY * accelY +
        accelZ * accelZ
      );


  bool freeFall =
      accelMagnitude <
      MPU_FREE_FALL;


  bool impact =
      accelMagnitude >
      MPU_IMPACT;


  if (freeFall)
  {
    if (lowAccelSince == 0)
    {
      lowAccelSince =
          millis();
    }

    if (
      millis() - lowAccelSince >=
      FALL_CONFIRM_TIME
    )
    {
      fallDetected = true;
    }
  }
  else
  {
    lowAccelSince = 0;
  }


  if (impact)
  {
    fallDetected = true;
  }


  mpuAbnormal =
      fallDetected;
}


// ============================================================
// GPS
// ============================================================

void readGPS()
{
  while (
    GPSSerial.available()
  )
  {
    gps.encode(
      GPSSerial.read()
    );
  }


  if (
    gps.location.isValid() &&
    gps.location.age() <
    GPS_STALE_TIME
  )
  {
    latitude =
        gps.location.lat();

    longitude =
        gps.location.lng();

    gpsValid = true;
  }
  else
  {
    gpsValid = false;
  }
}


// ============================================================
// MAX30102
// ============================================================

void calculateMAX30102()
{
  int32_t calculatedHR = 0;
  int32_t calculatedSpO2 = 0;

  int8_t validHR = 0;
  int8_t validSpO2 = 0;


  maxim_heart_rate_and_oxygen_saturation(
    irBuffer,
    SPO2_SAMPLES,
    redBuffer,

    &calculatedSpO2,
    &validSpO2,

    &calculatedHR,
    &validHR
  );


  if (
    validHR &&
    calculatedHR >= 40 &&
    calculatedHR <= 220
  )
  {
    heartRate =
        calculatedHR;

    hrValid = true;
  }
  else
  {
    hrValid = false;
  }


  if (
    validSpO2 &&
    calculatedSpO2 >= 70 &&
    calculatedSpO2 <= 100
  )
  {
    spo2 =
        calculatedSpO2;

    spo2Valid = true;
  }
  else
  {
    spo2Valid = false;
  }


  maxDataReady = true;


  /*
   * ----------------------------------------------------------
   * FC EFFECTIVE
   * ----------------------------------------------------------
   *
   * Si la FC simulée est anormale ET que l'AD8232
   * possède un signal :
   *
   * POT-FC est utilisée comme source d'affichage.
   *
   * Sinon MAX30102 est utilisé.
   */

  if (
    hrPotAbnormal &&
    ad8232Active
  )
  {
    effectiveHR =
        simulatedHR;

    hrSource =
        "POT-FC + AD8232";
  }
  else if (hrValid)
  {
    effectiveHR =
        heartRate;

    hrSource =
        "MAX30102";
  }
  else
  {
    effectiveHR = 0;

    hrSource =
        "AUCUNE";
  }


  /*
   * ----------------------------------------------------------
   * ANOMALIE MAX30102
   * ----------------------------------------------------------
   *
   * Le MAX30102 est indépendant du POT-FC.
   *
   * Il doit réellement fournir des données avant
   * d'être considéré comme une anomalie physiologique.
   */

  maxAbnormal = false;


  if (!spo2Valid)
  {
    maxAbnormal = true;
  }
  else if (
    spo2 <= SPO2_CRITICAL
  )
  {
    maxAbnormal = true;
  }


  if (!hrValid)
  {
    maxAbnormal = true;
  }
  else if (
    heartRate < HR_LOW ||
    heartRate > HR_HIGH
  )
  {
    maxAbnormal = true;
  }


  sampleCount = 0;
}


// ============================================================
// LECTURE MAX30102
// ============================================================

void readMAX30102()
{
  if (!maxOK)
  {
    hrValid = false;
    spo2Valid = false;
    maxDataReady = false;

    /*
     * Capteur absent = pas de condition
     * physiologique valide.
     */

    maxAbnormal = false;

    effectiveHR = 0;
    hrSource = "MAX30102 ABSENT";

    return;
  }


  max30102.check();


  while (
    max30102.available()
  )
  {
    if (
      sampleCount <
      SPO2_SAMPLES
    )
    {
      redBuffer[sampleCount] =
          max30102.getRed();

      irBuffer[sampleCount] =
          max30102.getIR();

      sampleCount++;
    }


    max30102.nextSample();


    if (
      sampleCount >=
      SPO2_SAMPLES
    )
    {
      calculateMAX30102();
    }
  }
}


// ============================================================
// LIEN GOOGLE MAPS
// ============================================================

String mapsLink()
{
  if (!gpsValid)
  {
    return "GPS indisponible";
  }


  String link =
      "https://maps.google.com/?q=";


  link +=
      String(
        latitude,
        6
      );

  link += ",";

  link +=
      String(
        longitude,
        6
      );


  return link;
}


// ============================================================
// ETAT
// ============================================================

String stateName()
{
  return state;
}


// ============================================================
// GSM
// ============================================================

void clearGSMBuffer()
{
  while (
    GSMSerial.available()
  )
  {
    GSMSerial.read();
  }
}


bool waitGSM(
  const String &expected,
  unsigned long timeout
)
{
  unsigned long start =
      millis();

  String response;


  while (
    millis() - start <
    timeout
  )
  {
    while (
      GSMSerial.available()
    )
    {
      char c =
          GSMSerial.read();

      Serial.write(c);

      response += c;


      if (
        response.indexOf(
          expected
        ) >= 0
      )
      {
        return true;
      }
    }

    yield();
  }


  return false;
}


// ============================================================
// GSM INIT
// ============================================================

void initGSM()
{
  clearGSMBuffer();


  GSMSerial.println(
    "AT"
  );


  if (
    !waitGSM(
      "OK",
      3000
    )
  )
  {
    gsmOK = false;

    Serial.println(
      "SIM800L : PAS DE REPONSE"
    );

    return;
  }


  GSMSerial.println(
    "ATE0"
  );

  waitGSM(
    "OK",
    2000
  );


  GSMSerial.println(
    "AT+CMGF=1"
  );


  if (
    !waitGSM(
      "OK",
      3000
    )
  )
  {
    gsmOK = false;

    Serial.println(
      "SIM800L : ERREUR SMS"
    );

    return;
  }


  GSMSerial.println(
    "AT+CSCS=\"GSM\""
  );

  waitGSM(
    "OK",
    2000
  );


  gsmOK = true;


  Serial.println(
    "SIM800L : READY"
  );
}


// ============================================================
// ENVOI SMS
// ============================================================

bool sendSMS(
  const String &number,
  const String &message
)
{
  if (!gsmOK)
  {
    return false;
  }


  if (
    number.length() == 0 ||
    number.indexOf('X') >= 0
  )
  {
    Serial.println(
      "Numero non configure"
    );

    return false;
  }


  clearGSMBuffer();


  GSMSerial.println(
    "AT+CMGF=1"
  );


  if (
    !waitGSM(
      "OK",
      3000
    )
  )
  {
    return false;
  }


  GSMSerial.print(
    "AT+CMGS=\""
  );

  GSMSerial.print(
    number
  );

  GSMSerial.println(
    "\""
  );


  if (
    !waitGSM(
      ">",
      5000
    )
  )
  {
    return false;
  }


  GSMSerial.print(
    message
  );


  GSMSerial.write(
    26
  );


  return waitGSM(
    "OK",
    15000
  );
}


// ============================================================
// SMS D'URGENCE
// ============================================================

bool sendEmergencyAlert()
{
  String message;


  message +=
      "MEDIWATCH NEO\n";

  message +=
      "Patient: " +
      patientName +
      "\n";

  message +=
      "ALERTE: 5/5 CONDITIONS\n";


  message +=
      "TEMP: " +
      String(
        temperature,
        1
      ) +
      " C\n";


  message +=
      "PRESSION: " +
      String(
        systolic,
        0
      ) +
      "/" +
      String(
        diastolic,
        0
      ) +
      " mmHg\n";


  message +=
      "FC: " +
      String(
        effectiveHR
      ) +
      " BPM\n";


  message +=
      "SOURCE FC: " +
      hrSource +
      "\n";


  message +=
      "SpO2: " +
      String(
        spo2Valid ?
        spo2 :
        0
      ) +
      " %\n";


  message +=
      "ECG AD8232: ";


  message +=
      ad8232Active ?
      "SIGNAL PRESENT" :
      "SIGNAL ABSENT";


  message += "\n";


  message +=
      "MPU6050: CHUTE/ANOMALIE\n";


  message +=
      "GPS: ";


  message +=
      mapsLink();


  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    "MEDIWATCH - ALERTE"
  );

  Serial.println(
    message
  );

  Serial.println(
    "================================"
  );


  if (!gsmOK)
  {
    Serial.println(
      "SMS : GSM INDISPONIBLE"
    );

    return false;
  }


  bool atLeastOneSuccess =
      false;


  bool result;


  result =
      sendSMS(
        contact1,
        message
      );


  if (result)
  {
    atLeastOneSuccess = true;
  }


  Serial.println(
    result ?
    "CONTACT 1 : OK" :
    "CONTACT 1 : ECHEC"
  );


  result =
      sendSMS(
        contact2,
        message
      );


  if (result)
  {
    atLeastOneSuccess = true;
  }


  Serial.println(
    result ?
    "CONTACT 2 : OK" :
    "CONTACT 2 : ECHEC"
  );


  result =
      sendSMS(
        contact3,
        message
      );


  if (result)
  {
    atLeastOneSuccess = true;
  }


  Serial.println(
    result ?
    "CONTACT 3 : OK" :
    "CONTACT 3 : ECHEC"
  );


  result =
      sendSMS(
        hospitalContact,
        message
      );


  if (result)
  {
    atLeastOneSuccess = true;
  }


  Serial.println(
    result ?
    "HOPITAL : OK" :
    "HOPITAL : ECHEC"
  );


  return atLeastOneSuccess;
}


// ============================================================
// EVALUATION 5/5
// ============================================================

void evaluateSystem()
{
  /*
   * ==========================================================
   * CONDITION 1
   * ==========================================================
   */

  bool conditionMPU =
      mpuAbnormal;


  /*
   * ==========================================================
   * CONDITION 2
   * ==========================================================
   */

  bool conditionTEMP =
      tempAbnormal;


  /*
   * ==========================================================
   * CONDITION 3
   * ==========================================================
   */

  bool conditionPRESSURE =
      pressureAbnormal;


  /*
   * ==========================================================
   * CONDITION 4
   * ==========================================================
   *
   * POT-FC anormal
   * ET
   * AD8232 actif
   */

  bool conditionCARDIAC =
      hrPotAbnormal &&
      ad8232Active;


  /*
   * ==========================================================
   * CONDITION 5
   * ==========================================================
   */

  bool conditionMAX =
      maxAbnormal;


  /*
   * ==========================================================
   * 5/5
   * ==========================================================
   */

  allAbnormal =
      conditionMPU &&
      conditionTEMP &&
      conditionPRESSURE &&
      conditionCARDIAC &&
      conditionMAX;


  /*
   * ==========================================================
   * SOS
   * ==========================================================
   */

  if (
    state == "SOS"
  )
  {
    if (
      millis() <
      sosUntil
    )
    {
      setRGB(
        false,
        false,
        true
      );

      buzzerUpdate();

      return;
    }

    state = "NORMAL";

    buzzerOff();
  }


  /*
   * ==========================================================
   * CRITICAL
   * ==========================================================
   */

  if (allAbnormal)
  {
    state =
        "CRITICAL";


    reason =
        "5/5 conditions anormales";


    setRGB(
      true,
      false,
      false
    );


    buzzerUpdate();


    if (
      allAbnormalSince == 0
    )
    {
      allAbnormalSince =
          millis();
    }


    /*
     * Confirmation 5 secondes
     */

    if (
      !emergencySent &&
      millis() -
      allAbnormalSince >=
      ALERT_CONFIRM_TIME
    )
    {
      bool smsOK =
          sendEmergencyAlert();


      /*
       * On ne considère l'alerte
       * comme envoyée que si au moins
       * un SMS a été accepté.
       */

      if (smsOK)
      {
        emergencySent = true;

        reason =
            "Alerte envoyee par SMS";
      }
      else
      {
        reason =
            "5/5 mais SMS echoue";
      }
    }


    return;
  }


  /*
   * ==========================================================
   * SI 5/5 CASSE
   * ==========================================================
   */

  allAbnormalSince = 0;


  /*
   * Dès qu'une des 5 conditions devient
   * normale, une future situation 5/5
   * pourra générer une nouvelle alerte.
   */

  emergencySent = false;


  bool somethingAbnormal =
      conditionMPU ||
      conditionTEMP ||
      conditionPRESSURE ||
      conditionCARDIAC ||
      conditionMAX;


  /*
   * ==========================================================
   * WARNING
   * ==========================================================
   */

  if (somethingAbnormal)
  {
    state =
        "WARNING";


    reason =
        "Anomalie detectee - SMS bloque";


    setRGB(
      true,
      true,
      false
    );


    buzzerOff();


    return;
  }


  /*
   * ==========================================================
   * NORMAL
   * ==========================================================
   */

  state =
      "NORMAL";


  reason =
      "Aucune anomalie";


  fallDetected = false;

  mpuAbnormal = false;


  setRGB(
    false,
    true,
    false
  );


  buzzerOff();
}


// ============================================================
// SOS
// ============================================================

void checkSOS()
{
  static bool previous =
      HIGH;


  bool current =
      digitalRead(
        PIN_SOS
      );


  if (
    previous == HIGH &&
    current == LOW &&
    millis() - lastSOS >=
    SOS_COOLDOWN
  )
  {
    lastSOS =
        millis();


    state =
        "SOS";


    reason =
        "SOS manuel";


    sosUntil =
        millis() + 3000;


    setRGB(
      false,
      false,
      true
    );


    buzzerShort();


    /*
     * SOS = SMS immédiat.
     */

    sendEmergencyAlert();
  }


  previous =
      current;
}


// ============================================================
// OLED
// ============================================================

void updateOLED()
{
  char line[32];


  oled.clearBuffer();


  oled.setFont(
    u8g2_font_6x10_tf
  );


  oled.drawStr(
    0,
    9,
    "MEDIWATCH NEO"
  );


  snprintf(
    line,
    sizeof(line),
    "ETAT: %s",
    stateName().c_str()
  );


  oled.drawStr(
    0,
    20,
    line
  );


  snprintf(
    line,
    sizeof(line),
    "FC: %d",
    effectiveHR
  );


  oled.drawStr(
    0,
    31,
    line
  );


  if (spo2Valid)
  {
    snprintf(
      line,
      sizeof(line),
      "SpO2: %d%%",
      spo2
    );
  }
  else
  {
    snprintf(
      line,
      sizeof(line),
      "SpO2: --"
    );
  }


  oled.drawStr(
    60,
    31,
    line
  );


  snprintf(
    line,
    sizeof(line),
    "T: %.1f C",
    temperature
  );


  oled.drawStr(
    0,
    42,
    line
  );


  snprintf(
    line,
    sizeof(line),
    "BP: %.0f/%.0f",
    systolic,
    diastolic
  );


  oled.drawStr(
    60,
    42,
    line
  );


  snprintf(
    line,
    sizeof(line),
    "5/5: %s",
    allAbnormal ?
    "OUI" :
    "NON"
  );


  oled.drawStr(
    0,
    53,
    line
  );


  snprintf(
    line,
    sizeof(line),
    "GPS:%s GSM:%s",
    gpsValid ?
    "OK" :
    "--",
    gsmOK ?
    "OK" :
    "--"
  );


  oled.drawStr(
    0,
    64,
    line
  );


  oled.sendBuffer();
}


// ============================================================
// JSON
// ============================================================

String makeJSON()
{
  String j = "{";


  j +=
      "\"patient\":\"" +
      patientName +
      "\",";


  j +=
      "\"temperature\":" +
      String(
        temperature,
        1
      ) +
      ",";


  j +=
      "\"systolic\":" +
      String(
        systolic,
        1
      ) +
      ",";


  j +=
      "\"diastolic\":" +
      String(
        diastolic,
        1
      ) +
      ",";


  j +=
      "\"simulatedHR\":" +
      String(
        simulatedHR
      ) +
      ",";


  j +=
      "\"heartRate\":" +
      String(
        heartRate
      ) +
      ",";


  j +=
      "\"effectiveHR\":" +
      String(
        effectiveHR
      ) +
      ",";


  j +=
      "\"spo2\":" +
      String(
        spo2
      ) +
      ",";


  j +=
      "\"ecg\":" +
      String(
        ecgValue
      ) +
      ",";


  j +=
      "\"latitude\":" +
      String(
        latitude,
        6
      ) +
      ",";


  j +=
      "\"longitude\":" +
      String(
        longitude,
        6
      ) +
      ",";


  j +=
      "\"gpsValid\":" +
      String(
        gpsValid ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"tempAbnormal\":" +
      String(
        tempAbnormal ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"pressureAbnormal\":" +
      String(
        pressureAbnormal ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"hrPotAbnormal\":" +
      String(
        hrPotAbnormal ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"ad8232Active\":" +
      String(
        ad8232Active ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"maxAbnormal\":" +
      String(
        maxAbnormal ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"mpuAbnormal\":" +
      String(
        mpuAbnormal ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"allAbnormal\":" +
      String(
        allAbnormal ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"spo2Valid\":" +
      String(
        spo2Valid ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"hrValid\":" +
      String(
        hrValid ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"mpuOK\":" +
      String(
        mpuOK ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"maxOK\":" +
      String(
        maxOK ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"gsmOK\":" +
      String(
        gsmOK ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"gpsValid\":" +
      String(
        gpsValid ?
        "true" :
        "false"
      ) +
      ",";


  j +=
      "\"state\":\"" +
      stateName() +
      "\",";


  j +=
      "\"reason\":\"" +
      reason +
      "\",";


  j +=
      "\"hrSource\":\"" +
      hrSource +
      "\"";


  j += "}";


  return j;
}


// ============================================================
// DASHBOARD HTML
// ============================================================

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>

<html lang="fr">

<head>

<meta charset="utf-8">

<meta
name="viewport"
content="width=device-width,initial-scale=1"
>

<title>MediWatch NEO</title>

<style>

*{
  box-sizing:border-box;
}

body{
  font-family:Arial,sans-serif;
  background:#07111f;
  color:white;
  margin:0;
}

main{
  max-width:1100px;
  margin:auto;
  padding:20px;
}

h1{
  margin-bottom:5px;
}

.subtitle{
  color:#9fb1c8;
}

.grid{
  display:grid;
  grid-template-columns:
  repeat(
    auto-fit,
    minmax(190px,1fr)
  );
  gap:12px;
}

.card{
  background:#102238;
  padding:16px;
  border-radius:14px;
  margin-top:12px;
}

.value{
  font-size:26px;
  font-weight:bold;
  margin-top:8px;
}

.good{
  color:#70f0a0;
}

.bad{
  color:#ff7180;
}

.warning{
  color:#ffd866;
}

.alert{
  padding:15px;
  border-radius:12px;
  margin-top:12px;
  background:#182d43;
}

.status{
  font-size:25px;
  font-weight:bold;
}

.small{
  color:#9fb1c8;
  font-size:13px;
}

a{
  color:#67b7ff;
}

</style>

</head>

<body>

<main>

<h1>MEDIWATCH NEO</h1>

<p class="subtitle">
Système de surveillance biométrique prototype
</p>

<p>
Patient :
<b id="patient">---</b>
</p>


<div class="grid">

<div class="card">

FC

<div
class="value"
id="hr"
>--</div>

<small id="source">--</small>

</div>


<div class="card">

SpO2

<div
class="value"
id="spo2"
>--</div>

</div>


<div class="card">

Température

<div
class="value"
id="temp"
>--</div>

</div>


<div class="card">

Pression

<div
class="value"
id="bp"
>--</div>

</div>


<div class="card">

ECG AD8232

<div
class="value"
id="ecg"
>--</div>

</div>


<div class="card">

GPS

<div id="gps">
Recherche...
</div>

</div>

</div>


<div class="card">

<h3>
Conditions 5/5
</h3>

<div id="conditions"></div>

</div>


<div class="card">

Etat :

<span
class="status"
id="state"
>NORMAL</span>

<p id="reason">
Aucune anomalie
</p>

</div>


<div
id="alert"
class="alert"
>
SYSTEME NORMAL
</div>


<div class="card">

<h3>
Modules
</h3>

<p id="modules">
Initialisation...
</p>

</div>


<p class="small">

MediWatch NEO V2.
<br>
Prototype pédagogique.
<br>
Température et pression simulées.
<br>
ECG non interprété médicalement.
<br>
Ne pas utiliser pour un diagnostic médical.

</p>

</main>


<script>

const $ =
id =>
document.getElementById(id);


function condition(
  name,
  bad
)
{
  return `
    <p class="${bad ? 'bad' : 'good'}">
      ${bad ? '🔴' : '🟢'}
      ${name}
    </p>
  `;
}


async function update()
{
  try
  {
    const response =
      await fetch(
        '/api/data',
        {
          cache:'no-store'
        }
      );


    const d =
      await response.json();


    $('patient').textContent =
      d.patient;


    $('hr').textContent =
      d.effectiveHR +
      ' BPM';


    $('source').textContent =
      'Source : ' +
      d.hrSource;


    $('spo2').textContent =
      d.spo2Valid
      ? d.spo2 + ' %'
      : '--';


    $('temp').textContent =
      d.temperature.toFixed(1) +
      ' °C';


    $('bp').textContent =
      Math.round(d.systolic) +
      '/' +
      Math.round(d.diastolic);


    $('ecg').textContent =
      d.ecg;


    $('state').textContent =
      d.state;


    $('reason').textContent =
      d.reason;


    $('conditions').innerHTML =

      condition(
        'MPU6050',
        d.mpuAbnormal
      )

      +

      condition(
        'POT-TMP',
        d.tempAbnormal
      )

      +

      condition(
        'POT-PRESSION',
        d.pressureAbnormal
      )

      +

      condition(
        'POT-FC + AD8232',
        d.hrPotAbnormal &&
        d.ad8232Active
      )

      +

      condition(
        'MAX30102',
        d.maxAbnormal
      );


    if(d.allAbnormal)
    {
      $('alert').textContent =
        '5/5 : SMS AUTOMATIQUE AUTORISE';
    }
    else if(d.state === 'SOS')
    {
      $('alert').textContent =
        'SOS : SMS IMMEDIAT';
    }
    else
    {
      $('alert').textContent =
        'SMS AUTOMATIQUE BLOQUE';
    }


    if(d.gpsValid)
    {
      const url =
        'https://maps.google.com/?q=' +
        d.latitude +
        ',' +
        d.longitude;


      $('gps').innerHTML =
        d.latitude.toFixed(6) +
        ', ' +
        d.longitude.toFixed(6) +
        ' <a target="_blank" href="' +
        url +
        '">Carte</a>';
    }
    else
    {
      $('gps').textContent =
        'GPS indisponible';
    }


    $('modules').innerHTML =

      'MPU6050 : ' +
      (d.mpuOK ? '🟢 OK' : '🔴 ERREUR') +

      '<br>MAX30102 : ' +
      (d.maxOK ? '🟢 OK' : '🔴 ERREUR') +

      '<br>SIM800L : ' +
      (d.gsmOK ? '🟢 OK' : '🔴 ERREUR') +

      '<br>GPS : ' +
      (d.gpsValid ? '🟢 FIX' : '🟡 RECHERCHE');
  }

  catch(error)
  {
    console.log(error);

    $('alert').textContent =
      'Connexion avec MediWatch perdue';
  }
}


update();

setInterval(
  update,
  500
);

</script>

</body>

</html>
)HTML";


// ============================================================
// SERVEUR WEB
// ============================================================

void handleRoot()
{
  server.send_P(
    200,
    "text/html",
    INDEX_HTML
  );
}


void handleData()
{
  server.send(
    200,
    "application/json",
    makeJSON()
  );
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );


  delay(300);


  Serial.println();
  Serial.println(
    "======================================"
  );

  Serial.println(
    "       MEDIWATCH NEO V2"
  );

  Serial.println(
    "======================================"
  );


  // ----------------------------------------------------------
  // GPIO
  // ----------------------------------------------------------

  pinMode(
    PIN_SOS,
    INPUT_PULLUP
  );

  pinMode(
    PIN_BUZZER,
    OUTPUT
  );


  pinMode(
    PIN_LED_R,
    OUTPUT
  );

  pinMode(
    PIN_LED_G,
    OUTPUT
  );

  pinMode(
    PIN_LED_B,
    OUTPUT
  );


  pinMode(
    PIN_ECG,
    INPUT
  );


  pinMode(
    PIN_TEMP_POT,
    INPUT
  );

  pinMode(
    PIN_PRESS_POT,
    INPUT
  );

  pinMode(
    PIN_HR_POT,
    INPUT
  );


  // ----------------------------------------------------------
  // ADC
  // ----------------------------------------------------------

  analogReadResolution(
    12
  );


  analogSetPinAttenuation(
    PIN_ECG,
    ADC_11db
  );

  analogSetPinAttenuation(
    PIN_TEMP_POT,
    ADC_11db
  );

  analogSetPinAttenuation(
    PIN_PRESS_POT,
    ADC_11db
  );

  analogSetPinAttenuation(
    PIN_HR_POT,
    ADC_11db
  );


  // ----------------------------------------------------------
  // ETAT INITIAL
  // ----------------------------------------------------------

  setRGB(
    false,
    true,
    false
  );

  buzzerOff();


  // ----------------------------------------------------------
  // I2C
  // ----------------------------------------------------------

  Wire.begin(
    PIN_SDA,
    PIN_SCL
  );


  Wire.setClock(
    400000
  );


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  oled.begin();


  oled.clearBuffer();

  oled.setFont(
    u8g2_font_6x10_tf
  );


  oled.drawStr(
    0,
    20,
    "MEDIWATCH NEO V2"
  );


  oled.drawStr(
    0,
    35,
    "Initialisation..."
  );


  oled.sendBuffer();


  // ----------------------------------------------------------
  // MPU6050
  // ----------------------------------------------------------

  mpuOK =
      mpu.begin();


  if (mpuOK)
  {
    mpu.setAccelerometerRange(
      MPU6050_RANGE_8_G
    );

    mpu.setGyroRange(
      MPU6050_RANGE_500_DEG
    );

    mpu.setFilterBandwidth(
      MPU6050_BAND_21_HZ
    );


    Serial.println(
      "MPU6050 : OK"
    );
  }
  else
  {
    Serial.println(
      "MPU6050 : ERREUR"
    );
  }


  // ----------------------------------------------------------
  // MAX30102
  // ----------------------------------------------------------

  maxOK =
      max30102.begin(
        Wire,
        I2C_SPEED_FAST
      );


  if (maxOK)
  {
    max30102.setup(
      60,
      4,
      2,
      100,
      411,
      4096
    );


    max30102.setPulseAmplitudeRed(
      0x24
    );


    max30102.setPulseAmplitudeIR(
      0x24
    );


    max30102.clearFIFO();


    Serial.println(
      "MAX30102 : OK"
    );
  }
  else
  {
    Serial.println(
      "MAX30102 : ERREUR"
    );
  }


  // ----------------------------------------------------------
  // GPS NEO-6M
  // ----------------------------------------------------------

  GPSSerial.begin(
    9600,
    SERIAL_8N1,
    PIN_GPS_RX,
    PIN_GPS_TX
  );


  Serial.println(
    "GPS : UART READY"
  );


  // ----------------------------------------------------------
  // SIM800L
  // ----------------------------------------------------------

  GSMSerial.begin(
    9600,
    SERIAL_8N1,
    PIN_GSM_RX,
    PIN_GSM_TX
  );


  Serial.println(
    "SIM800L : INITIALISATION"
  );


  initGSM();


  // ----------------------------------------------------------
  // WIFI ACCESS POINT
  // ----------------------------------------------------------

  WiFi.mode(
    WIFI_AP
  );


  bool apOK =
      WiFi.softAP(
        WIFI_SSID,
        WIFI_PASSWORD
      );


  if (apOK)
  {
    Serial.println(
      "WIFI AP : OK"
    );
  }
  else
  {
    Serial.println(
      "WIFI AP : ERREUR"
    );
  }


  // ----------------------------------------------------------
  // SERVEUR
  // ----------------------------------------------------------

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/api/data",
    HTTP_GET,
    handleData
  );


  server.begin();


  // ----------------------------------------------------------
  // ECG TIMER
  // ----------------------------------------------------------

  ecgWindowStart =
      millis();


  // ----------------------------------------------------------
  // INFORMATIONS
  // ----------------------------------------------------------

  Serial.println();

  Serial.println(
    "======================================"
  );

  Serial.println(
    "       MEDIWATCH NEO V2 READY"
  );

  Serial.println(
    "======================================"
  );


  Serial.print(
    "SSID : "
  );

  Serial.println(
    WIFI_SSID
  );


  Serial.print(
    "IP   : "
  );

  Serial.println(
    WiFi.softAPIP()
  );


  Serial.printf(
    "MPU6050  : %s\n",
    mpuOK ?
    "OK" :
    "ERREUR"
  );


  Serial.printf(
    "MAX30102 : %s\n",
    maxOK ?
    "OK" :
    "ERREUR"
  );


  Serial.printf(
    "SIM800L  : %s\n",
    gsmOK ?
    "OK" :
    "ERREUR"
  );


  Serial.println(
    "======================================"
  );


  oled.clearBuffer();


  oled.drawStr(
    0,
    20,
    "MEDIWATCH NEO"
  );


  oled.drawStr(
    0,
    35,
    "SYSTEM READY"
  );


  oled.sendBuffer();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  unsigned long now =
      millis();


  // ----------------------------------------------------------
  // SERVEUR
  // ----------------------------------------------------------

  server.handleClient();


  // ----------------------------------------------------------
  // GPS
  // ----------------------------------------------------------

  readGPS();


  // ----------------------------------------------------------
  // ECG
  // ----------------------------------------------------------

  /*
   * L'AD8232 doit être lu rapidement.
   */

  readECG();


  // ----------------------------------------------------------
  // CAPTEURS
  // ----------------------------------------------------------

  if (
    now - lastSensorRead >=
    SENSOR_INTERVAL
  )
  {
    lastSensorRead =
        now;


    readTemperature();

    readPressure();

    readHeartRatePot();

    readMPU();
  }


  // ----------------------------------------------------------
  // MAX30102
  // ----------------------------------------------------------

  readMAX30102();


  // ----------------------------------------------------------
  // SOS
  // ----------------------------------------------------------

  checkSOS();


  // ----------------------------------------------------------
  // EVALUATION
  // ----------------------------------------------------------

  evaluateSystem();


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  if (
    now - lastDisplay >=
    DISPLAY_INTERVAL
  )
  {
    lastDisplay =
        now;

    updateOLED();
  }


  /*
   * Aucun delay().
   *
   * L'ESP32 reste réactif :
   *
   * GPS
   * GSM
   * WiFi
   * AD8232
   * MAX30102
   * MPU6050
   * SOS
   * OLED
   */
}
