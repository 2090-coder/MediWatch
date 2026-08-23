/*
 * MEDIWATCH NEO
 * ESP32 NodeMCU-32S
 *
 * PINS DE LA CARTE :
 * VP = GPIO36, VN = GPIO39, P34 = GPIO34, P35 = GPIO35
 *
 * - POT-TMP    -> VP  (simulation TMP117)
 * - POT-PRESS  -> P35 (simulation pression)
 * - POT-FC     -> VN  (simulation FC)
 * - AD8232 OUT -> P34 (ECG reel)
 * - MPU6050, MAX30102, OLED -> I2C P21/P22
 * - GPS -> P16/P17
 * - GSM -> P26/P27
 * - SOS -> P32
 * - Buzzer -> P14
 * - RGB -> P25/P33/P13
 *
 * SMS automatique :
 * UNIQUEMENT quand les 5 conditions sont anormales en meme temps :
 * 1) MPU6050
 * 2) POT-TMP
 * 3) POT-PRESSION
 * 4) POT-FC + signal AD8232
 * 5) MAX30102
 *
 * SOS est une exception : bouton SOS = SMS immediat.
 *
 * PROTOTYPE : temperature, pression et FC sont simulees.
 * ECG, FC/SpO2, GPS et chute sont experimentaux.
 * Ne pas utiliser pour un diagnostic medical.
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

// ======================= PROTOTYPES =============================
void sendEmergencyAlert();
bool waitGSM(const String& expected, unsigned long timeout);
bool sendSMS(const String& number, const String& message);

// ========================= BROCHES CARTE =========================
#define PIN_SDA       21
#define PIN_SCL       22
#define PIN_ECG       34      // P34
#define PIN_TEMP_POT  36      // VP
#define PIN_PRESS_POT 35      // P35
#define PIN_HR_POT    39      // VN
#define PIN_GPS_RX    16      // P16
#define PIN_GPS_TX    17      // P17
#define PIN_GSM_RX    26      // P26
#define PIN_GSM_TX    27      // P27
#define PIN_SOS       32      // P32
#define PIN_BUZZER    14      // P14
#define PIN_LED_R     25      // P25
#define PIN_LED_G     33      // P33
#define PIN_LED_B     13      // P13

// ========================= PARAMETRES ============================
const char* WIFI_SSID = "MEDIWATCH";
const char* WIFI_PASSWORD = "mediwatch123";
String patientName = "UWASE";
String contact1 = "+243XXXXXXXXX";
String contact2 = "+243XXXXXXXXX";
String contact3 = "+243XXXXXXXXX";
String hospitalContact = "+243XXXXXXXXX";

const float TEMP_MIN = 36.0;
const float TEMP_MAX = 37.9;
const float PRESS_MIN = 90.0;
const float PRESS_WARNING = 140.0;
const int HR_LOW = 50;
const int HR_HIGH = 120;
const int SPO2_CRITICAL = 90;
const float MPU_FALL_G = 2.5;
const float MPU_IMPACT_G = 22.0;
const unsigned long SENSOR_INTERVAL = 100;
const unsigned long DISPLAY_INTERVAL = 500;
const unsigned long ALERT_CONFIRM_TIME = 5000;
const unsigned long GPS_STALE_TIME = 5000;
const unsigned long SOS_COOLDOWN = 3000;
const unsigned long FALL_CONFIRM_TIME = 250;

// =========================== OBJETS =============================
HardwareSerial GPSSerial(1);
HardwareSerial GSMSerial(2);
WebServer server(80);
TinyGPSPlus gps;
Adafruit_MPU6050 mpu;
MAX30105 max30102;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// =========================== ETAT ===============================
bool mpuOK = false;
bool maxOK = false;
bool gsmOK = false;
bool gpsValid = false;
bool tempAbnormal = false;
bool pressureAbnormal = false;
bool hrPotAbnormal = false;
bool ad8232Active = false;
bool maxAbnormal = false;
bool mpuAbnormal = false;
bool allAbnormal = false;
bool emergencySent = false;
bool fallDetected = false;
bool beepState = false;

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
bool hrValid = false;
bool spo2Valid = false;
String hrSource = "MAX30102";
String state = "NORMAL";
String reason = "Aucune alerte";

unsigned long lastSensorRead = 0;
unsigned long lastDisplay = 0;
unsigned long lastBeep = 0;
unsigned long allAbnormalSince = 0;
unsigned long lastSOS = 0;
unsigned long sosUntil = 0;
unsigned long lowAccelSince = 0;

const int SPO2_SAMPLES = 100;
uint32_t irBuffer[SPO2_SAMPLES];
uint32_t redBuffer[SPO2_SAMPLES];
int sampleCount = 0;

// ========================= OUTILS ================================
void setRGB(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r);
  digitalWrite(PIN_LED_G, g);
  digitalWrite(PIN_LED_B, b);
}

void buzzerOff() {
  digitalWrite(PIN_BUZZER, LOW);
  beepState = false;
}

void buzzerUpdate() {
  if (millis() - lastBeep >= 400) {
    lastBeep = millis();
    beepState = !beepState;
    digitalWrite(PIN_BUZZER, beepState);
  }
}

void buzzerShortStart() {
  digitalWrite(PIN_BUZZER, HIGH);
  lastBeep = millis();
}

String stateName() {
  if (state == "SOS") return "SOS";
  if (state == "CRITICAL") return "CRITICAL";
  if (state == "WARNING") return "WARNING";
  return "NORMAL";
}

// ========================= TEMPERATURE ===========================
void readTemperature() {
  int raw = analogRead(PIN_TEMP_POT);
  temperature = 30.0 + (15.0 * raw / 4095.0);
  tempAbnormal = (temperature < TEMP_MIN || temperature > TEMP_MAX);
}

// ========================== PRESSION =============================
void readPressure() {
  int raw = analogRead(PIN_PRESS_POT);
  systolic = 70.0 + (130.0 * raw / 4095.0);
  diastolic = systolic * 0.65;
  pressureAbnormal = (systolic < PRESS_MIN || systolic >= PRESS_WARNING);
}

// ============================ ECG ================================
void readECG() {
  ecgValue = analogRead(PIN_ECG);
  // Presence de signal seulement. Aucune interpretation medicale.
  ad8232Active = (ecgValue > 50 && ecgValue < 4090);
}

// ========================== POT FC ===============================
void readHeartRatePot() {
  int raw = analogRead(PIN_HR_POT);
  simulatedHR = 40 + (140 * raw / 4095);
  hrPotAbnormal = (simulatedHR < HR_LOW || simulatedHR > HR_HIGH);
}

// ============================ MPU6050 ============================
void readMPU() {
  if (!mpuOK) {
    mpuAbnormal = false;
    return;
  }

  sensors_event_t accel, gyro, temp;
  mpu.getEvent(&accel, &gyro, &temp);
  accelX = accel.acceleration.x;
  accelY = accel.acceleration.y;
  accelZ = accel.acceleration.z;
  accelMagnitude = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);

  bool freeFall = accelMagnitude < MPU_FALL_G;
  bool impact = accelMagnitude > MPU_IMPACT_G;

  if (freeFall) {
    if (lowAccelSince == 0) lowAccelSince = millis();
    if (millis() - lowAccelSince >= FALL_CONFIRM_TIME) fallDetected = true;
  } else {
    lowAccelSince = 0;
  }

  if (impact) fallDetected = true;
  mpuAbnormal = fallDetected;
}

// ============================ GPS =================================
void readGPS() {
  while (GPSSerial.available()) gps.encode(GPSSerial.read());

  if (gps.location.isValid() && gps.location.age() < GPS_STALE_TIME) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    gpsValid = true;
  } else {
    gpsValid = false;
  }
}

// ========================== MAX30102 ===============================
void readMAX30102() {
  if (!maxOK) {
    hrValid = false;
    spo2Valid = false;
    maxAbnormal = true;
    return;
  }

  max30102.check();

  while (max30102.available()) {
    redBuffer[sampleCount] = max30102.getRed();
    irBuffer[sampleCount] = max30102.getIR();
    sampleCount++;
    max30102.nextSample();

    if (sampleCount >= SPO2_SAMPLES) {
      int32_t calculatedHR;
      int32_t calculatedSpO2;
      int8_t validHR;
      int8_t validSpO2;

      maxim_heart_rate_and_oxygen_saturation(
          irBuffer, SPO2_SAMPLES, redBuffer,
          &calculatedSpO2, &validSpO2,
          &calculatedHR, &validHR
      );

      if (validHR && calculatedHR >= 40 && calculatedHR <= 220) {
        heartRate = calculatedHR;
        hrValid = true;
      } else {
        hrValid = false;
      }

      if (validSpO2 && calculatedSpO2 >= 70 && calculatedSpO2 <= 100) {
        spo2 = calculatedSpO2;
        spo2Valid = true;
      } else {
        spo2Valid = false;
      }

      // POT-FC anormal = simulation volontaire de l'anomalie.
      // POT-FC normal = MAX30102 reste la source FC si valide.
      if (hrPotAbnormal) {
        effectiveHR = simulatedHR;
        hrSource = "POT-FC";
      } else if (hrValid) {
        effectiveHR = heartRate;
        hrSource = "MAX30102";
      } else {
        effectiveHR = 0;
        hrSource = "AUCUNE";
      }

      maxAbnormal = false;
      if (spo2Valid && spo2 <= SPO2_CRITICAL) maxAbnormal = true;

      if (!hrPotAbnormal) {
        if (!hrValid) maxAbnormal = true;
        else if (heartRate < HR_LOW || heartRate > HR_HIGH) maxAbnormal = true;
      }

      sampleCount = 0;
    }
  }
}

// ========================= LOGIQUE 5/5 ============================
void evaluateSystem() {
  // SOS garde la priorite pendant 3 secondes.
  if (state == "SOS") {
    if (millis() < sosUntil) {
      setRGB(false, false, true);
      buzzerUpdate();
      return;
    }
    buzzerOff();
    state = "NORMAL";
  }

  // Condition 4 = POT-FC anormal + AD8232 fournissant un signal.
  bool condition4 = hrPotAbnormal && ad8232Active;

  allAbnormal =
      mpuAbnormal &&
      tempAbnormal &&
      pressureAbnormal &&
      condition4 &&
      maxAbnormal;

  if (allAbnormal) {
    state = "CRITICAL";
    reason = "5/5 conditions anormales";
    setRGB(true, false, false);
    buzzerUpdate();

    if (allAbnormalSince == 0) allAbnormalSince = millis();

    if (!emergencySent && millis() - allAbnormalSince >= ALERT_CONFIRM_TIME) {
      sendEmergencyAlert();
      emergencySent = true;
    }
    return;
  }

  allAbnormalSince = 0;

  bool somethingAbnormal =
      mpuAbnormal || tempAbnormal || pressureAbnormal ||
      hrPotAbnormal || maxAbnormal;

  if (somethingAbnormal) {
    state = "WARNING";
    reason = "Anomalie detectee - SMS bloque";
    setRGB(true, true, false);
    buzzerOff();
    return;
  }

  state = "NORMAL";
  reason = "Aucune anomalie";
  emergencySent = false;
  fallDetected = false;
  setRGB(false, true, false);
  buzzerOff();
}

// ============================ GSM =================================
bool waitGSM(const String& expected, unsigned long timeout) {
  unsigned long start = millis();
  String response;

  while (millis() - start < timeout) {
    while (GSMSerial.available()) {
      char c = GSMSerial.read();
      Serial.write(c);
      response += c;
      if (response.indexOf(expected) >= 0) return true;
    }
  }
  return false;
}

void initGSM() {
  GSMSerial.println("AT");
  if (!waitGSM("OK", 2500)) {
    gsmOK = false;
    return;
  }

  GSMSerial.println("ATE0");
  waitGSM("OK", 1500);
  GSMSerial.println("AT+CMGF=1");
  gsmOK = waitGSM("OK", 2500);
  Serial.println(gsmOK ? "GSM READY" : "GSM ERROR");
}

bool sendSMS(const String& number, const String& message) {
  if (!gsmOK || number.indexOf('X') >= 0) return false;

  GSMSerial.println("AT+CMGF=1");
  if (!waitGSM("OK", 2500)) return false;

  GSMSerial.print("AT+CMGS=\"");
  GSMSerial.print(number);
  GSMSerial.println("\"");

  if (!waitGSM(">", 5000)) return false;

  GSMSerial.print(message);
  GSMSerial.write(26);
  return waitGSM("OK", 15000);
}

String mapsLink() {
  if (!gpsValid) return "GPS indisponible";
  return "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
}

void sendEmergencyAlert() {
  String message =
      "MEDIWATCH NEO - ALERTE\n"
      "Patient: " + patientName + "\n"
      "5/5 conditions anormales.\n"
      "TEMP: " + String(temperature, 1) + " C\n"
      "PRESSION: " + String(systolic, 0) + "/" + String(diastolic, 0) + " mmHg\n"
      "FC: " + String(effectiveHR) + " BPM (" + hrSource + ")\n"
      "SpO2: " + String(spo2) + " %\n"
      "ECG AD8232: signal acquis\n"
      "MPU: anomalie/chute\n"
      "GPS: " + mapsLink();

  Serial.println("\n========== ALERTE MEDIWATCH ==========");
  Serial.println(message);
  Serial.println("======================================");

  if (!gsmOK) return;
  sendSMS(contact1, message);
  sendSMS(contact2, message);
  sendSMS(contact3, message);
  sendSMS(hospitalContact, message);
}

// ============================= SOS ================================
void checkSOS() {
  static bool previous = HIGH;
  bool current = digitalRead(PIN_SOS);

  if (previous == HIGH && current == LOW && millis() - lastSOS >= SOS_COOLDOWN) {
    lastSOS = millis();
    state = "SOS";
    reason = "SOS manuel";
    sosUntil = millis() + 3000;
    setRGB(false, false, true);
    buzzerShortStart();
    sendEmergencyAlert();
  }

  previous = current;
}

// ============================ OLED ================================
void updateOLED() {
  char line[32];
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 9, "MEDIWATCH NEO");
  snprintf(line, sizeof(line), "ETAT: %s", stateName().c_str());
  oled.drawStr(0, 20, line);
  snprintf(line, sizeof(line), "FC: %d %s", effectiveHR, hrSource.c_str());
  oled.drawStr(0, 31, line);
  snprintf(line, sizeof(line), "SpO2: %d%%", spo2Valid ? spo2 : 0);
  oled.drawStr(0, 42, line);
  snprintf(line, sizeof(line), "T: %.1f BP: %.0f/%.0f", temperature, systolic, diastolic);
  oled.drawStr(0, 53, line);
  oled.sendBuffer();
}

// ============================ WEB =================================
String makeJSON() {
  String j = "{";
  j += "\"patient\":\"" + patientName + "\",";
  j += "\"temperature\":" + String(temperature, 1) + ",";
  j += "\"systolic\":" + String(systolic, 1) + ",";
  j += "\"diastolic\":" + String(diastolic, 1) + ",";
  j += "\"simulatedHR\":" + String(simulatedHR) + ",";
  j += "\"heartRate\":" + String(heartRate) + ",";
  j += "\"effectiveHR\":" + String(effectiveHR) + ",";
  j += "\"spo2\":" + String(spo2) + ",";
  j += "\"ecg\":" + String(ecgValue) + ",";
  j += "\"latitude\":" + String(latitude, 6) + ",";
  j += "\"longitude\":" + String(longitude, 6) + ",";
  j += "\"gpsValid\":" + String(gpsValid ? "true" : "false") + ",";
  j += "\"tempAbnormal\":" + String(tempAbnormal ? "true" : "false") + ",";
  j += "\"pressureAbnormal\":" + String(pressureAbnormal ? "true" : "false") + ",";
  j += "\"hrPotAbnormal\":" + String(hrPotAbnormal ? "true" : "false") + ",";
  j += "\"ad8232Active\":" + String(ad8232Active ? "true" : "false") + ",";
  j += "\"maxAbnormal\":" + String(maxAbnormal ? "true" : "false") + ",";
  j += "\"mpuAbnormal\":" + String(mpuAbnormal ? "true" : "false") + ",";
  j += "\"allAbnormal\":" + String(allAbnormal ? "true" : "false") + ",";
  j += "\"spo2Valid\":" + String(spo2Valid ? "true" : "false") + ",";
  j += "\"state\":\"" + stateName() + "\",";
  j += "\"reason\":\"" + reason + "\",";
  j += "\"hrSource\":\"" + hrSource + "\"";
  j += "}";
  return j;
}

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>MediWatch NEO</title>
<style>
body{font-family:Arial;background:#07111f;color:white;margin:0}main{max-width:1000px;margin:auto;padding:20px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px}
.card{background:#102238;padding:16px;border-radius:14px;margin-top:12px}.value{font-size:26px;font-weight:bold;margin-top:8px}
.good{color:#70f0a0}.bad{color:#ff7180}.alert{padding:15px;border-radius:12px;margin-top:12px}
</style></head><body><main><h1>MEDIWATCH NEO</h1><p>Patient : <b id="patient">---</b></p>
<div class="grid"><div class="card">FC<div class="value" id="hr">--</div><small id="source">--</small></div>
<div class="card">SpO2<div class="value" id="spo2">--</div></div><div class="card">Temperature simulee<div class="value" id="temp">--</div></div>
<div class="card">Pression simulee<div class="value" id="bp">--</div></div><div class="card">ECG AD8232<div class="value" id="ecg">--</div></div>
<div class="card">GPS<div id="gps">Recherche...</div></div></div>
<div class="card"><h3>Conditions 5/5</h3><div id="conditions"></div></div>
<div class="card">Etat : <span class="value" id="state">NORMAL</span><p id="reason">Aucune alerte</p></div>
<div id="alert" class="alert">SYSTEME NORMAL</div><p>Prototype MediWatch NEO. Ne pas utiliser pour un diagnostic medical.</p></main>
<script>
const $=id=>document.getElementById(id);
function condition(name,bad){return '<p class="'+(bad?'bad':'good')+'">'+(bad?'🔴 ':'🟢 ')+name+'</p>';}
async function update(){try{const r=await fetch('/api/data',{cache:'no-store'}),d=await r.json();
$('patient').textContent=d.patient;$('hr').textContent=d.effectiveHR+' BPM';$('source').textContent='Source : '+d.hrSource;
$('spo2').textContent=d.spo2Valid?d.spo2+' %':'--';$('temp').textContent=d.temperature.toFixed(1)+' °C';
$('bp').textContent=Math.round(d.systolic)+'/'+Math.round(d.diastolic);$('ecg').textContent=d.ecg;$('state').textContent=d.state;$('reason').textContent=d.reason;
$('conditions').innerHTML=condition('MPU6050',d.mpuAbnormal)+condition('POT-TMP',d.tempAbnormal)+condition('POT-PRESSION',d.pressureAbnormal)+condition('POT-FC + AD8232',d.hrPotAbnormal&&d.ad8232Active)+condition('MAX30102',d.maxAbnormal);
$('alert').textContent=d.allAbnormal?'🔴 5/5 : SMS AUTOMATIQUE AUTORISE':'🟡 SMS AUTOMATIQUE BLOQUE';
if(d.gpsValid){$('gps').innerHTML=d.latitude.toFixed(6)+', '+d.longitude.toFixed(6)+' <a target="_blank" href="https://maps.google.com/?q='+d.latitude+','+d.longitude+'">Carte</a>'}else{$('gps').textContent='GPS indisponible'}
}catch(e){}}
update();setInterval(update,500);
</script></body></html>
)HTML";

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }
void handleData() { server.send(200, "application/json", makeJSON()); }

// =========================== INITIALISATION =======================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_SOS, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_ECG, INPUT);
  pinMode(PIN_TEMP_POT, INPUT);
  pinMode(PIN_PRESS_POT, INPUT);
  pinMode(PIN_HR_POT, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ECG, ADC_11db);
  analogSetPinAttenuation(PIN_TEMP_POT, ADC_11db);
  analogSetPinAttenuation(PIN_PRESS_POT, ADC_11db);
  analogSetPinAttenuation(PIN_HR_POT, ADC_11db);

  setRGB(false, true, false);
  buzzerOff();
  Wire.begin(PIN_SDA, PIN_SCL);

  oled.begin();
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 20, "MEDIWATCH NEO");
  oled.drawStr(0, 35, "Initialisation...");
  oled.sendBuffer();

  mpuOK = mpu.begin();
  if (mpuOK) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  maxOK = max30102.begin(Wire, I2C_SPEED_FAST);
  if (maxOK) {
    max30102.setup(60, 4, 2, 100, 411, 4096);
    max30102.setPulseAmplitudeRed(0x24);
    max30102.setPulseAmplitudeIR(0x24);
    max30102.clearFIFO();
  }

  GPSSerial.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  GSMSerial.begin(9600, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
  initGSM();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleData);
  server.begin();

  Serial.println();
  Serial.print("WiFi : "); Serial.println(WIFI_SSID);
  Serial.print("IP : "); Serial.println(WiFi.softAPIP());
  Serial.printf("MPU6050: %s | MAX30102: %s | GSM: %s\n",
                mpuOK ? "OK" : "ERREUR",
                maxOK ? "OK" : "ERREUR",
                gsmOK ? "OK" : "ERREUR");

  oled.clearBuffer();
  oled.drawStr(0, 20, "MEDIWATCH NEO");
  oled.drawStr(0, 35, "SYSTEM READY");
  oled.sendBuffer();
}

// =============================== LOOP =============================
void loop() {
  unsigned long now = millis();
  server.handleClient();
  readGPS();

  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readTemperature();
    readPressure();
    readHeartRatePot();
    readECG();
    readMPU();
  }

  readMAX30102();
  checkSOS();
  evaluateSystem();

  if (now - lastDisplay >= DISPLAY_INTERVAL) {
    lastDisplay = now;
    updateOLED();
  }

  // Aucun delay() : la boucle reste reactive.
  if (state == "SOS" && millis() - lastBeep >= 250) buzzerOff();
}
