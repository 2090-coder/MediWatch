/*
 * ============================================================
 * MEDIWATCH V2.0 - ESP32
 * ============================================================
 * Prototype de bracelet medical connecte.
 *
 * IMPORTANT : prototype uniquement.
 * - La pression arterielle est SIMULEE par potentiometre.
 * - L'ECG est acquis mais n'est pas interprete medicalement.
 * - FC/SpO2 sont experimentaux et dependent de la qualite du signal.
 * - La detection de chute est experimentale.
 * - Ne pas utiliser pour diagnostic ou traitement medical.
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <Adafruit_TMP117.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <MAX30105.h>
#include "spo2_algorithm.h"
#include <TinyGPS++.h>
#include <math.h>

#define I2C_SDA 21
#define I2C_SCL 22
#define ECG_PIN 34
#define PRESSURE_PIN 35
#define GPS_RX 16
#define GPS_TX 17
#define GSM_RX 26
#define GSM_TX 27
#define SOS_PIN 32
#define BUZZER_PIN 14
#define LED_R 25
#define LED_G 33
#define LED_B 13

#define GPS_BAUD 9600
#define GSM_BAUD 9600
#define MAX_SAMPLES 100

const char* WIFI_SSID = "MEDIWATCH";
const char* WIFI_PASSWORD = "mediwatch123";

String patientName = "UWASE";
String contact1 = "+243XXXXXXXXX";
String contact2 = "+243XXXXXXXXX";
String contact3 = "+243XXXXXXXXX";
String hospitalContact = "+243XXXXXXXXX";

const float PRESSURE_WARNING = 140.0;
const float PRESSURE_CRITICAL = 160.0;
const float TEMP_WARNING = 38.0;
const float TEMP_CRITICAL = 39.0;
const int SPO2_WARNING = 94;
const int SPO2_CRITICAL = 90;
const int HR_LOW = 50;
const int HR_HIGH = 120;

const unsigned long PRESSURE_CONFIRM_MS = 5000;
const unsigned long SOS_COOLDOWN_MS = 3000;
const unsigned long FALL_COOLDOWN_MS = 10000;
const unsigned long FALL_CONFIRM_MS = 250;
const unsigned long GPS_STALE_MS = 5000;

HardwareSerial GPSSerial(1);
HardwareSerial GSMSerial(2);
WebServer server(80);
TinyGPSPlus gps;
Adafruit_TMP117 tmp117;
Adafruit_MPU6050 mpu;
MAX30105 max30102;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

bool tmpOK = false;
bool mpuOK = false;
bool maxOK = false;
bool gsmOK = false;

float temperatureC = 0;
float systolic = 120;
float diastolic = 78;
int heartRate = 0;
int spo2 = 0;
int ecgValue = 0;
float latitude = 0;
float longitude = 0;
float accelMagnitude = 9.81;
float accelX = 0;
float accelY = 0;
float accelZ = 0;

bool hrValid = false;
bool spo2Valid = false;
bool gpsValid = false;
bool fallDetected = false;

uint32_t irBuffer[MAX_SAMPLES];
uint32_t redBuffer[MAX_SAMPLES];
int sampleIndex = 0;

unsigned long lastBeatTime = 0;
unsigned long lastDisplay = 0;
unsigned long lastTemp = 0;
unsigned long lastMPU = 0;
unsigned long lastECG = 0;
unsigned long lastPressure = 0;
unsigned long lastGPS = 0;
unsigned long criticalSince = 0;
unsigned long lastFall = 0;
unsigned long lowAccelSince = 0;
unsigned long lastBeep = 0;
bool beepState = false;

bool emergencySent = false;
String alertReason = "Aucune alerte";

unsigned long lastSOS = 0;

enum SystemState {
  NORMAL,
  WARNING,
  CRITICAL,
  SOS
};

SystemState state = NORMAL;

const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MediWatch V2</title>
<style>
body{margin:0;background:#07111f;color:#eaf2ff;font:16px Arial,sans-serif}.wrap{max-width:1050px;margin:auto;padding:18px}.top{display:flex;justify-content:space-between;align-items:center}.brand{font-size:28px;font-weight:800}.conn{padding:8px 12px;border-radius:18px;background:#16452f;color:#70f0a0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px}.card{background:#102238;border:1px solid #213a55;border-radius:16px;padding:16px;margin-top:12px}.label{font-size:12px;color:#9eb3c9}.value{font-size:29px;font-weight:bold;margin-top:8px}.muted{color:#9bb0c8}.alert{padding:14px;border-radius:14px;background:#16452f;margin-top:12px}.warning{background:#5a4a14}.danger{background:#641c26}.sos{background:#183c73}.ecg{width:100%;height:180px;background:#06101a;border-radius:10px}a{color:#70f0a0}.foot{text-align:center;color:#71869d;font-size:12px;padding:20px}
</style></head><body><main class="wrap">
<div class="top"><div><div class="brand">⌚ MEDIWATCH V2</div><div class="muted">Patient : <b id="patient">---</b></div></div><div id="conn" class="conn">● CONNEXION</div></div>
<div class="grid">
<div class="card"><div class="label">❤️ FREQUENCE CARDIAQUE</div><div class="value"><span id="hr">--</span> BPM</div></div>
<div class="card"><div class="label">🫁 SpO₂</div><div class="value"><span id="spo2">--</span> %</div></div>
<div class="card"><div class="label">🌡️ TEMPERATURE</div><div class="value"><span id="temp">--</span> °C</div></div>
<div class="card"><div class="label">🩺 PRESSION SIMULEE</div><div class="value" id="bp">--/--</div></div>
</div>
<div class="card"><div class="label">📈 ECG — Acquisition</div><canvas id="ecg" class="ecg" width="1000" height="180"></canvas></div>
<div class="grid">
<div class="card"><div class="label">📍 GPS</div><div id="gps" class="muted">Recherche...</div><p><a id="map" target="_blank">Voir sur Google Maps</a></p></div>
<div class="card"><div class="label">🚨 ETAT</div><div id="state" class="value">NORMAL</div><div id="reason" class="muted">Aucune alerte</div><div id="fall" class="muted"></div></div>
</div>
<div id="alert" class="alert">🟢 Systeme normal</div>
<div class="foot">MediWatch V2 · Prototype · Pression arterielle simulee · Ne pas utiliser pour diagnostic medical</div>
</main>
<script>
const $=id=>document.getElementById(id),points=[],canvas=$("ecg"),ctx=canvas.getContext("2d");
function draw(){ctx.clearRect(0,0,canvas.width,canvas.height);ctx.beginPath();points.forEach((v,i)=>{const x=i*canvas.width/Math.max(1,points.length-1);const y=90-(v-2048)*.06;i?ctx.lineTo(x,y):ctx.moveTo(x,y)});ctx.strokeStyle="#65f3ad";ctx.lineWidth=2;ctx.stroke()}
async function update(){try{const r=await fetch('/api/data',{cache:'no-store'}),d=await r.json();$("patient").textContent=d.patient;$("hr").textContent=d.heartRateValid?d.heartRate:'--';$("spo2").textContent=d.spo2Valid?d.spo2:'--';$("temp").textContent=Number(d.temperature).toFixed(1);$("bp").textContent=Math.round(d.systolic)+'/'+Math.round(d.diastolic);$("state").textContent=d.state;$("reason").textContent=d.reason;$("fall").textContent=d.fallDetected?'⚠️ Chute detectee':'Aucune chute detectee';$("conn").textContent='● CONNECTE';const a=$("alert");if(d.state==='CRITICAL'){a.textContent='🔴 ALERTE CRITIQUE';a.className='alert danger'}else if(d.state==='WARNING'){a.textContent='🟡 ATTENTION';a.className='alert warning'}else if(d.state==='SOS'){a.textContent='🔵 SOS ACTIVE';a.className='alert sos'}else{a.textContent='🟢 Systeme normal';a.className='alert'}if(d.gpsValid){$("gps").textContent=Number(d.latitude).toFixed(6)+', '+Number(d.longitude).toFixed(6);$("map").href='https://maps.google.com/?q='+d.latitude+','+d.longitude}else{$("gps").textContent='Position GPS indisponible';$("map").removeAttribute('href')}points.push(d.ecg);if(points.length>300)points.shift();draw()}catch(e){$("conn").textContent='● HORS LIGNE'}}
update();setInterval(update,500);
</script></body></html>
)HTML";

const char* stateName() {
  switch (state) {
    case WARNING: return "WARNING";
    case CRITICAL: return "CRITICAL";
    case SOS: return "SOS";
    default: return "NORMAL";
  }
}

void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R, r ? HIGH : LOW);
  digitalWrite(LED_G, g ? HIGH : LOW);
  digitalWrite(LED_B, b ? HIGH : LOW);
}

void stopBuzzer() {
  digitalWrite(BUZZER_PIN, LOW);
  beepState = false;
}

void buzzerShort() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(120);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerAlarm() {
  if (millis() - lastBeep >= 400) {
    lastBeep = millis();
    beepState = !beepState;
    digitalWrite(BUZZER_PIN, beepState ? HIGH : LOW);
  }
}

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
    delay(1);
  }
  return false;
}

void initGSM() {
  while (GSMSerial.available()) GSMSerial.read();
  GSMSerial.println("AT");
  if (!waitGSM("OK", 2500)) { gsmOK = false; return; }
  GSMSerial.println("ATE0");
  waitGSM("OK", 2000);
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
  if (!gpsValid) return "GPS POSITION UNAVAILABLE";
  return "https://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);
}

String escapeJSON(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  return value;
}

String jsonData() {
  String json = "{";
  json += "\"patient\":\"" + escapeJSON(patientName) + "\",";
  json += "\"heartRate\":" + String(heartRate) + ",";
  json += "\"heartRateValid\":" + String(hrValid ? "true" : "false") + ",";
  json += "\"spo2\":" + String(spo2) + ",";
  json += "\"spo2Valid\":" + String(spo2Valid ? "true" : "false") + ",";
  json += "\"temperature\":" + String(temperatureC, 1) + ",";
  json += "\"systolic\":" + String(systolic, 1) + ",";
  json += "\"diastolic\":" + String(diastolic, 1) + ",";
  json += "\"ecg\":" + String(ecgValue) + ",";
  json += "\"latitude\":" + String(latitude, 6) + ",";
  json += "\"longitude\":" + String(longitude, 6) + ",";
  json += "\"gpsValid\":" + String(gpsValid ? "true" : "false") + ",";
  json += "\"fallDetected\":" + String(fallDetected ? "true" : "false") + ",";
  json += "\"state\":\"" + stateName() + "\",";
  json += "\"reason\":\"" + escapeJSON(alertReason) + "\"";
  json += "}";
  return json;
}

void sendEmergencyAlert(const String& reason) {
  String message = "MEDIWATCH - ALERTE MEDICALE\n";
  message += "Patient: " + patientName + "\n";
  message += "Motif: " + reason + "\n";
  message += "Pression SIMULEE: " + String(systolic, 0) + "/" + String(diastolic, 0) + " mmHg\n";
  if (hrValid) message += "FC: " + String(heartRate) + " BPM\n";
  if (spo2Valid) message += "SpO2: " + String(spo2) + " %\n";
  message += "Temperature: " + String(temperatureC, 1) + " C\n";
  message += "LOCALISATION:\n" + mapsLink();

  Serial.println("\n========== MEDIWATCH ALERT ==========");
  Serial.println(message);
  Serial.println("=====================================\n");

  if (!gsmOK) return;
  sendSMS(contact1, message);
  delay(500);
  sendSMS(contact2, message);
  delay(500);
  sendSMS(contact3, message);
  delay(500);
  sendSMS(hospitalContact, message);
}

void readTemperature() {
  if (!tmpOK) return;
  sensors_event_t event;
  if (tmp117.getEvent(&event)) temperatureC = event.temperature;
}

void readPressure() {
  int value = analogRead(PRESSURE_PIN);
  systolic = 80.0 + (120.0 * value / 4095.0);
  diastolic = systolic * 0.65;
}

void readECG() {
  ecgValue = analogRead(ECG_PIN);
}

void readGPS() {
  while (GPSSerial.available()) gps.encode(GPSSerial.read());
  if (gps.location.isValid() && gps.location.age() < GPS_STALE_MS) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    gpsValid = true;
  } else {
    gpsValid = false;
  }
}

void readMPU() {
  if (!mpuOK) return;
  sensors_event_t accel, gyro, temp;
  if (!mpu.getEvent(&accel, &gyro, &temp)) return;

  accelX = accel.acceleration.x;
  accelY = accel.acceleration.y;
  accelZ = accel.acceleration.z;
  accelMagnitude = sqrtf(accelX * accelX + accelY * accelY + accelZ * accelZ);

  static bool impactDetected = false;

  if (accelMagnitude > 22.0) {
    impactDetected = true;
    lowAccelSince = millis();
  }

  if (accelMagnitude < 2.5) {
    if (lowAccelSince == 0) lowAccelSince = millis();
    if (millis() - lowAccelSince >= FALL_CONFIRM_MS && impactDetected && millis() - lastFall >= FALL_COOLDOWN_MS) {
      fallDetected = true;
      lastFall = millis();
      impactDetected = false;
      lowAccelSince = 0;
    }
  } else if (accelMagnitude > 4.0) {
    if (millis() - lowAccelSince > 1000) lowAccelSince = 0;
  }
}

void readMAX30102() {
  if (!maxOK) return;
  max30102.check();
  while (max30102.available()) {
    redBuffer[sampleIndex] = max30102.getRed();
    irBuffer[sampleIndex] = max30102.getIR();
    max30102.nextSample();
    sampleIndex++;

    if (sampleIndex >= MAX_SAMPLES) {
      int32_t spo2Value = 0;
      int32_t hrValue = 0;
      int8_t spo2ValidFlag = 0;
      int8_t hrValidFlag = 0;

      maxim_heart_rate_and_oxygen_saturation(
        irBuffer, MAX_SAMPLES, redBuffer,
        &spo2Value, &spo2ValidFlag,
        &hrValue, &hrValidFlag
      );

      if (hrValidFlag && hrValue >= 40 && hrValue <= 220) {
        heartRate = hrValue;
        hrValid = true;
      }

      if (spo2ValidFlag && spo2Value >= 70 && spo2Value <= 100) {
        spo2 = spo2Value;
        spo2Valid = true;
      }

      sampleIndex = 0;
    }
  }
}

void checkSOS() {
  static bool previous = HIGH;
  bool current = digitalRead(SOS_PIN);

  if (previous == HIGH && current == LOW && millis() - lastSOS >= SOS_COOLDOWN_MS) {
    lastSOS = millis();
    state = SOS;
    alertReason = "SOS manuel";
    setLED(false, false, true);
    buzzerShort();
    sendEmergencyAlert("SOS MANUEL");
    emergencySent = true;
  }
  previous = current;
}

void processState() {
  if (state == SOS) {
    setLED(false, false, true);
    return;
  }

  bool critical = false;
  bool warning = false;
  String reason = "Aucune alerte";

  if (systolic >= PRESSURE_CRITICAL) {
    critical = true;
    reason = "Pression simulee critique";
  } else if (systolic >= PRESSURE_WARNING) {
    warning = true;
    reason = "Pression simulee elevee";
  }

  if (temperatureC >= TEMP_CRITICAL) {
    critical = true;
    reason = "Temperature critique";
  } else if (temperatureC >= TEMP_WARNING && !critical) {
    warning = true;
    if (reason == "Aucune alerte") reason = "Temperature elevee";
  }

  if (spo2Valid) {
    if (spo2 <= SPO2_CRITICAL) {
      critical = true;
      reason = "SpO2 critique";
    } else if (spo2 <= SPO2_WARNING && !critical) {
      warning = true;
      if (reason == "Aucune alerte") reason = "SpO2 faible";
    }
  }

  if (hrValid && (heartRate < HR_LOW || heartRate > HR_HIGH) && !critical) {
    warning = true;
    if (reason == "Aucune alerte") reason = "Frequence cardiaque anormale";
  }

  if (fallDetected) {
    critical = true;
    reason = "Chute detectee";
  }

  alertReason = reason;

  if (critical) {
    if (state != CRITICAL) criticalSince = millis();
    state = CRITICAL;
    setLED(true, false, false);
    buzzerAlarm();

    bool immediate = fallDetected;
    if (!emergencySent && (immediate || millis() - criticalSince >= PRESSURE_CONFIRM_MS)) {
      sendEmergencyAlert(reason);
      emergencySent = true;
    }
  } else if (warning) {
    state = WARNING;
    criticalSince = 0;
    setLED(true, true, false);
    stopBuzzer();
  } else {
    state = NORMAL;
    criticalSince = 0;
    emergencySent = false;
    fallDetected = false;
    alertReason = "Aucune alerte";
    setLED(false, true, false);
    stopBuzzer();
  }
}

void updateOLED() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 9, "MEDIWATCH V2");
  oled.drawStr(88, 9, stateName());

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "HR: %s", hrValid ? String(heartRate).c_str() : "--");
  oled.drawStr(0, 22, buffer);
  snprintf(buffer, sizeof(buffer), "SpO2: %s", spo2Valid ? String(spo2).c_str() : "--");
  oled.drawStr(0, 33, buffer);
  snprintf(buffer, sizeof(buffer), "TEMP: %.1f C", temperatureC);
  oled.drawStr(0, 44, buffer);
  snprintf(buffer, sizeof(buffer), "BP*: %.0f/%.0f", systolic, diastolic);
  oled.drawStr(0, 55, buffer);
  oled.sendBuffer();
}

void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleData() {
  server.send(200, "application/json", jsonData());
}

void handleStatus() {
  String json = "{";
  json += "\"tmp117\":" + String(tmpOK ? "true" : "false") + ",";
  json += "\"mpu6050\":" + String(mpuOK ? "true" : "false") + ",";
  json += "\"max30102\":" + String(maxOK ? "true" : "false") + ",";
  json += "\"gps\":" + String(gpsValid ? "true" : "false") + ",";
  json += "\"gsm\":" + String(gsmOK ? "true" : "false") + ",";
  json += "\"uptime\":" + String(millis());
  json += "}";
  server.send(200, "application/json", json);
}

void initSensors() {
  oled.begin();
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(10, 25, "MEDIWATCH V2");
  oled.drawStr(10, 42, "Initialisation...");
  oled.sendBuffer();

  tmpOK = tmp117.begin();
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

  Serial.printf("TMP117:%s MPU6050:%s MAX30102:%s\n", tmpOK ? "OK" : "ERR", mpuOK ? "OK" : "ERR", maxOK ? "OK" : "ERR");
}

void initWeb() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.begin();

  Serial.print("Dashboard: http://");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(SOS_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(ECG_PIN, INPUT);
  pinMode(PRESSURE_PIN, INPUT);

  setLED(false, true, false);
  stopBuzzer();

  Wire.begin(I2C_SDA, I2C_SCL);
  initSensors();

  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  GSMSerial.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(2000);
  initGSM();
  initWeb();

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(15, 20, "MEDIWATCH V2");
  oled.drawStr(15, 35, "SYSTEM READY");
  oled.drawStr(15, 50, "PROTOTYPE");
  oled.sendBuffer();
  delay(1000);
}

void loop() {
  unsigned long now = millis();

  server.handleClient();

  if (now - lastGPS >= 20) {
    lastGPS = now;
    readGPS();
  }

  if (now - lastTemp >= 1000) {
    lastTemp = now;
    readTemperature();
  }

  if (now - lastPressure >= 100) {
    lastPressure = now;
    readPressure();
  }

  if (now - lastMPU >= 50) {
    lastMPU = now;
    readMPU();
  }

  if (now - lastECG >= 20) {
    lastECG = now;
    readECG();
  }

  readMAX30102();
  checkSOS();
  processState();

  if (now - lastDisplay >= 500) {
    lastDisplay = now;
    updateOLED();
  }

  delay(1);
}
