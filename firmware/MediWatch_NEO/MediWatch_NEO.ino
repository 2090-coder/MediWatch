/*
 * ============================================================
 * MEDIWATCH NEO - ESP32
 * ============================================================
 * Prototype de bracelet medical connecte.
 *
 * ARCHITECTURE DE SIMULATION NEO
 * - TMP117 absent -> potentiometre temperature
 * - Pression arterielle -> potentiometre pression
 * - AD8232 reste REEL et son signal ECG est affiche
 * - Potentiometre FC simule une anomalie de frequence cardiaque
 * - Si le potentiometre FC est normal, la FC du MAX30102 est utilisee
 * - MPU6050 -> mouvement/chute experimentale
 * - MAX30102 -> FC + SpO2 experimentaux
 * - GPS -> position reelle
 * - GSM -> SMS
 *
 * REGLE D'ALERTE NEO
 * Un SMS automatique n'est envoye que si les 5 conditions
 * medicales/simulees sont anormales simultanement :
 *   1. MPU6050 anormal / chute detectee
 *   2. Potentiometre temperature anormal
 *   3. Potentiometre pression anormal
 *   4. Potentiometre FC anormal + contexte AD8232
 *   5. MAX30102 anormal
 *
 * Le bouton SOS reste une exception : SOS = envoi immediat.
 *
 * IMPORTANT : prototype uniquement.
 * La pression et la temperature sont simulees.
 * L'ECG n'est pas interprete medicalement.
 * FC/SpO2 et chute sont experimentaux.
 * Ne pas utiliser pour diagnostic ou traitement medical.
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

// ============================ PINS ============================
#define I2C_SDA 21
#define I2C_SCL 22

#define ECG_PIN 34
#define TEMP_SIM_PIN 36
#define PRESSURE_PIN 35
#define HR_SIM_PIN 39

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

// ============================ WIFI =============================
const char* WIFI_SSID = "MEDIWATCH";
const char* WIFI_PASSWORD = "mediwatch123";

// ============================ PATIENT ==========================
String patientName = "UWASE";
String contact1 = "+243XXXXXXXXX";
String contact2 = "+243XXXXXXXXX";
String contact3 = "+243XXXXXXXXX";
String hospitalContact = "+243XXXXXXXXX";

// ============================ SEUILS ===========================
const float TEMP_NORMAL_MIN = 36.0;
const float TEMP_NORMAL_MAX = 37.9;
const float TEMP_WARNING = 38.0;

const float PRESSURE_WARNING = 140.0;
const float PRESSURE_CRITICAL = 160.0;
const float PRESSURE_LOW = 90.0;

const int HR_LOW = 50;
const int HR_HIGH = 120;
const int SPO2_CRITICAL = 90;
const int SPO2_WARNING = 94;

const float MPU_FALL_G = 2.5;
const float MPU_IMPACT_G = 22.0;

const unsigned long ALL_ABNORMAL_CONFIRM_MS = 5000;
const unsigned long SOS_COOLDOWN_MS = 3000;
const unsigned long FALL_COOLDOWN_MS = 10000;
const unsigned long FALL_CONFIRM_MS = 250;
const unsigned long GPS_STALE_MS = 5000;

// ============================ OBJETS ===========================
HardwareSerial GPSSerial(1);
HardwareSerial GSMSerial(2);
WebServer server(80);
TinyGPSPlus gps;
Adafruit_MPU6050 mpu;
MAX30105 max30102;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

bool mpuOK = false;
bool maxOK = false;
bool gsmOK = false;

// ============================ DONNEES ==========================
float simulatedTemperature = 36.5;
float systolic = 120.0;
float diastolic = 78.0;
int simulatedHR = 75;
int effectiveHR = 0;
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

// ====================== ETATS DE DYSFONCTION ===================
bool tempAbnormal = false;
bool pressureAbnormal = false;
bool hrPotAbnormal = false;
bool maxAbnormal = false;
bool mpuAbnormal = false;
bool ad8232Active = false;
bool allAbnormal = false;

String selectedHRSource = "MAX30102";
String alertReason = "Aucune alerte";

// ============================ TIMERS ===========================
unsigned long lastDisplay = 0;
unsigned long lastTemp = 0;
unsigned long lastMPU = 0;
unsigned long lastECG = 0;
unsigned long lastPressure = 0;
unsigned long lastHRSim = 0;
unsigned long lastGPS = 0;
unsigned long lastBeep = 0;
unsigned long allAbnormalSince = 0;
unsigned long lastSOS = 0;
unsigned long lastFall = 0;
unsigned long lowAccelSince = 0;

bool beepState = false;
bool emergencySent = false;

uint32_t irBuffer[MAX_SAMPLES];
uint32_t redBuffer[MAX_SAMPLES];
int sampleIndex = 0;

enum SystemState { NORMAL, WARNING, CRITICAL, SOS };
SystemState state = NORMAL;

// ============================ WEB ===============================
const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MediWatch NEO</title>
<style>
body{margin:0;background:#07111f;color:#eaf2ff;font:16px Arial,sans-serif}.wrap{max-width:1100px;margin:auto;padding:18px}.top{display:flex;justify-content:space-between;align-items:center}.brand{font-size:28px;font-weight:800}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px}.card{background:#102238;border:1px solid #213a55;border-radius:16px;padding:16px;margin-top:12px}.label{font-size:12px;color:#9eb3c9}.value{font-size:26px;font-weight:bold;margin-top:8px}.muted{color:#9bb0c8}.alert{padding:14px;border-radius:14px;background:#16452f;margin-top:12px}.warning{background:#5a4a14}.danger{background:#641c26}.sos{background:#183c73}.bad{color:#ff7180}.good{color:#70f0a0}.ecg{width:100%;height:180px;background:#06101a;border-radius:10px}.foot{text-align:center;color:#71869d;font-size:12px;padding:20px}
</style></head><body><main class="wrap">
<div class="top"><div><div class="brand">⌚ MEDIWATCH NEO</div><div class="muted">Patient : <b id="patient">---</b></div></div><div id="conn">● CONNEXION</div></div>
<div class="grid">
<div class="card"><div class="label">FC EFFECTIVE</div><div class="value"><span id="hr">--</span> BPM</div><div id="hrsrc" class="muted">Source : --</div></div>
<div class="card"><div class="label">SpO₂ MAX30102</div><div class="value"><span id="spo2">--</span> %</div></div>
<div class="card"><div class="label">TEMP. SIMULEE</div><div class="value"><span id="temp">--</span> °C</div><div id="tempok"></div></div>
<div class="card"><div class="label">PRESSION SIMULEE</div><div class="value" id="bp">--/--</div><div id="bpok"></div></div>
</div>
<div class="card"><div class="label">ECG — AD8232 REEL</div><div class="value" id="ecgval">--</div><div class="muted">Le signal analogique est acquis et affiche; aucune interpretation medicale.</div></div>
<div class="grid">
<div class="card"><div class="label">MPU6050</div><div id="mpu">--</div></div>
<div class="card"><div class="label">POT FC</div><div id="hrpot">--</div></div>
<div class="card"><div class="label">MAX30102</div><div id="max">--</div></div>
<div class="card"><div class="label">LOGIQUE D'ALERTE</div><div id="logic">--</div></div>
</div>
<div class="card"><div class="label">GPS</div><div id="gps" class="muted">Recherche...</div><p><a id="map" target="_blank">Voir sur Google Maps</a></p></div>
<div class="card"><div class="label">ETAT</div><div id="state" class="value">NORMAL</div><div id="reason" class="muted">Aucune alerte</div></div>
<div id="alert" class="alert">🟢 Systeme normal</div>
<div class="foot">MediWatch NEO · Prototype · Temperature/pression/FC simulees · Ne pas utiliser pour diagnostic medical</div>
</main>
<script>
const $=id=>document.getElementById(id);
async function update(){try{const r=await fetch('/api/data',{cache:'no-store'}),d=await r.json();
$('patient').textContent=d.patient;$('hr').textContent=d.effectiveHR;$('hrsrc').textContent='Source : '+d.hrSource;$('spo2').textContent=d.spo2Valid?d.spo2:'--';$('temp').textContent=d.temperature.toFixed(1);$('bp').textContent=Math.round(d.systolic)+'/'+Math.round(d.diastolic);$('ecgval').textContent=d.ecg;
$('tempok').textContent=d.tempAbnormal?'🔴 POT-TMP ANORMAL':'🟢 POT-TMP NORMAL';$('bpok').textContent=d.pressureAbnormal?'🔴 POT-PRESSION ANORMAL':'🟢 POT-PRESSION NORMAL';$('hrpot').textContent=d.simulatedHR+' BPM — '+(d.hrPotAbnormal?'🔴 ANORMAL':'🟢 NORMAL');$('mpu').textContent=d.mpuAbnormal?'🔴 ANORMAL / CHUTE':'🟢 NORMAL';$('max').textContent=d.maxAbnormal?'🔴 FC/SpO₂ ANORMAL':'🟢 NORMAL';$('logic').textContent=d.allAbnormal?'🔴 5/5 ANORMAUX':'🟢 Tous les capteurs ne sont pas anormaux';$('state').textContent=d.state;$('reason').textContent=d.reason;$('conn').textContent='● CONNECTE';
const a=$('alert');if(d.state==='SOS'){a.textContent='🔵 SOS ACTIVE';a.className='alert sos'}else if(d.allAbnormal){a.textContent='🔴 CONDITIONS D’URGENCE REUNIES';a.className='alert danger'}else if(d.state==='WARNING'){a.textContent='🟡 ANOMALIE DETECTEE — SMS NON ENVOYE';a.className='alert warning'}else{a.textContent='🟢 Systeme normal';a.className='alert'}
if(d.gpsValid){$('gps').textContent=d.latitude.toFixed(6)+', '+d.longitude.toFixed(6);$('map').href='https://maps.google.com/?q='+d.latitude+','+d.longitude}else{$('gps').textContent='Position GPS indisponible';$('map').removeAttribute('href')}}catch(e){$('conn').textContent='● HORS LIGNE'}}
update();setInterval(update,500);
</script></body></html>
)HTML";

const char* stateName(){
  switch(state){case WARNING:return "WARNING";case CRITICAL:return "CRITICAL";case SOS:return "SOS";default:return "NORMAL";}
}

void setLED(bool r,bool g,bool b){digitalWrite(LED_R,r);digitalWrite(LED_G,g);digitalWrite(LED_B,b);}
void stopBuzzer(){digitalWrite(BUZZER_PIN,LOW);beepState=false;}
void buzzerShort(){digitalWrite(BUZZER_PIN,HIGH);delay(120);digitalWrite(BUZZER_PIN,LOW);}
void buzzerAlarm(){if(millis()-lastBeep>=400){lastBeep=millis();beepState=!beepState;digitalWrite(BUZZER_PIN,beepState);}}

bool waitGSM(const String& expected,unsigned long timeout){
  unsigned long start=millis();String response;
  while(millis()-start<timeout){while(GSMSerial.available()){char c=GSMSerial.read();Serial.write(c);response+=c;if(response.indexOf(expected)>=0)return true;}delay(1);}return false;
}

void initGSM(){
  while(GSMSerial.available())GSMSerial.read();GSMSerial.println("AT");
  if(!waitGSM("OK",2500)){gsmOK=false;return;}GSMSerial.println("ATE0");waitGSM("OK",2000);GSMSerial.println("AT+CMGF=1");gsmOK=waitGSM("OK",2500);Serial.println(gsmOK?"GSM READY":"GSM ERROR");
}

bool sendSMS(const String& number,const String& message){
  if(!gsmOK||number.indexOf('X')>=0)return false;GSMSerial.println("AT+CMGF=1");if(!waitGSM("OK",2500))return false;GSMSerial.print("AT+CMGS=\"");GSMSerial.print(number);GSMSerial.println("\"");if(!waitGSM(">",5000))return false;GSMSerial.print(message);GSMSerial.write(26);return waitGSM("OK",15000);
}

String mapsLink(){if(!gpsValid)return "GPS POSITION UNAVAILABLE";return "https://maps.google.com/?q="+String(latitude,6)+","+String(longitude,6);}

String jsonEscape(String s){s.replace("\\","\\\\");s.replace("\"","\\\"");return s;}

void sendEmergencyAlert(const String& reason){
  String msg="MEDIWATCH NEO - ALERTE\n";
  msg+="Patient: "+patientName+"\n";
  msg+="Motif: "+reason+"\n";
  msg+="TEMP simulee: "+String(simulatedTemperature,1)+" C\n";
  msg+="PRESSION simulee: "+String(systolic,0)+"/"+String(diastolic,0)+" mmHg\n";
  msg+="FC effective: "+String(effectiveHR)+" BPM ("+selectedHRSource+")\n";
  if(spo2Valid)msg+="SpO2: "+String(spo2)+" %\n";
  msg+="ECG AD8232: acquis\n";
  msg+="MPU: anormal\n";
  msg+="LOCALISATION:\n"+mapsLink();
  Serial.println("\n========== MEDIWATCH NEO ALERT ==========");Serial.println(msg);Serial.println("==========================================\n");
  if(!gsmOK)return;sendSMS(contact1,msg);delay(500);sendSMS(contact2,msg);delay(500);sendSMS(contact3,msg);delay(500);sendSMS(hospitalContact,msg);
}

void readTemperatureSimulation(){
  int raw=analogRead(TEMP_SIM_PIN);
  simulatedTemperature=30.0+(20.0*raw/4095.0);
  tempAbnormal=(simulatedTemperature<TEMP_NORMAL_MIN||simulatedTemperature>=TEMP_WARNING);
}

void readPressure(){
  int raw=analogRead(PRESSURE_PIN);
  systolic=70.0+(130.0*raw/4095.0);
  diastolic=systolic*0.65;
  pressureAbnormal=(systolic<PRESSURE_LOW||systolic>=PRESSURE_WARNING);
}

void readHRSimulation(){
  int raw=analogRead(HR_SIM_PIN);
  simulatedHR=40+(160*raw/4095);
  hrPotAbnormal=(simulatedHR<HR_LOW||simulatedHR>HR_HIGH);

  // Si le potentiometre FC est normal, on conserve la FC MAX30102.
  // S'il simule une anomalie, cette valeur devient la FC de reference.
  if(hrPotAbnormal){effectiveHR=simulatedHR;selectedHRSource="POT-FC";}
  else if(hrValid){effectiveHR=heartRate;selectedHRSource="MAX30102";}
  else {effectiveHR=simulatedHR;selectedHRSource="POT-FC (secours)";}
}

void readECG(){ecgValue=analogRead(ECG_PIN);ad8232Active=true;}

void readGPS(){
  while(GPSSerial.available())gps.encode(GPSSerial.read());
  if(gps.location.isValid()&&gps.location.age()<GPS_STALE_MS){latitude=gps.location.lat();longitude=gps.location.lng();gpsValid=true;}else gpsValid=false;
}

void readMPU(){
  if(!mpuOK)return;
  sensors_event_t accel,gyro,temp;if(!mpu.getEvent(&accel,&gyro,&temp))return;
  accelX=accel.acceleration.x;accelY=accel.acceleration.y;accelZ=accel.acceleration.z;
  accelMagnitude=sqrtf(accelX*accelX+accelY*accelY+accelZ*accelZ);
  static bool impact=false;
  if(accelMagnitude>MPU_IMPACT_G){impact=true;lowAccelSince=millis();}
  if(accelMagnitude<MPU_FALL_G){if(lowAccelSince==0)lowAccelSince=millis();if(millis()-lowAccelSince>=FALL_CONFIRM_MS&&impact&&millis()-lastFall>=FALL_COOLDOWN_MS){fallDetected=true;lastFall=millis();impact=false;lowAccelSince=0;}}
  else if(accelMagnitude>4.0){if(millis()-lowAccelSince>1000)lowAccelSince=0;}
  mpuAbnormal=fallDetected;
}

void readMAX30102(){
  if(!maxOK)return;
  max30102.check();
  while(max30102.available()){
    redBuffer[sampleIndex]=max30102.getRed();irBuffer[sampleIndex]=max30102.getIR();max30102.nextSample();sampleIndex++;
    if(sampleIndex>=MAX_SAMPLES){
      int32_t spo2Value=0,hrValue=0;int8_t spo2Flag=0,hrFlag=0;
      maxim_heart_rate_and_oxygen_saturation(irBuffer,MAX_SAMPLES,redBuffer,&spo2Value,&spo2Flag,&hrValue,&hrFlag);
      if(hrFlag&&hrValue>=40&&hrValue<=220){heartRate=hrValue;hrValid=true;}
      if(spo2Flag&&spo2Value>=70&&spo2Value<=100){spo2=spo2Value;spo2Valid=true;}
      sampleIndex=0;
    }
  }
  bool hrBad=hrValid&&(heartRate<HR_LOW||heartRate>HR_HIGH);
  bool spo2Bad=spo2Valid&&(spo2<=SPO2_CRITICAL);
  maxAbnormal=hrBad||spo2Bad;
}

void processState(){
  // La condition NEO est volontairement stricte : 5/5 anormaux.
  allAbnormal=mpuAbnormal&&tempAbnormal&&pressureAbnormal&&hrPotAbnormal&&maxAbnormal;

  if(state==SOS){setLED(false,false,true);buzzerAlarm();return;}

  bool anyWarning=tempAbnormal||pressureAbnormal||hrPotAbnormal||maxAbnormal||mpuAbnormal;
  alertReason="Aucune alerte";

  if(allAbnormal){
    if(allAbnormalSince==0)allAbnormalSince=millis();
    state=CRITICAL;alertReason="5/5 conditions anormales";setLED(true,false,false);buzzerAlarm();
    if(!emergencySent&&millis()-allAbnormalSince>=ALL_ABNORMAL_CONFIRM_MS){sendEmergencyAlert(alertReason);emergencySent=true;}
  }else if(anyWarning){
    state=WARNING;allAbnormalSince=0;setLED(true,true,false);stopBuzzer();
    if(tempAbnormal)alertReason="Temperature simulee anormale";
    else if(pressureAbnormal)alertReason="Pression simulee anormale";
    else if(hrPotAbnormal)alertReason="POT-FC anormal";
    else if(maxAbnormal)alertReason="MAX30102 anormal";
    else if(mpuAbnormal)alertReason="Chute MPU6050";
  }else{
    state=NORMAL;allAbnormalSince=0;emergencySent=false;fallDetected=false;mpuAbnormal=false;alertReason="Aucune alerte";setLED(false,true,false);stopBuzzer();
  }
}

void checkSOS(){
  static bool previous=HIGH;bool current=digitalRead(SOS_PIN);
  if(previous==HIGH&&current==LOW&&millis()-lastSOS>=SOS_COOLDOWN_MS){lastSOS=millis();state=SOS;alertReason="SOS manuel";setLED(false,false,true);buzzerShort();sendEmergencyAlert("SOS MANUEL");emergencySent=true;}
  previous=current;
}

String jsonData(){
  String j="{";
  j+="\"patient\":\""+jsonEscape(patientName)+"\",";
  j+="\"temperature\":"+String(simulatedTemperature,1)+",";
  j+="\"systolic\":"+String(systolic,1)+",";
  j+="\"diastolic\":"+String(diastolic,1)+",";
  j+="\"simulatedHR\":"+String(simulatedHR)+",";
  j+="\"heartRate\":"+String(heartRate)+",";
  j+="\"effectiveHR\":"+String(effectiveHR)+",";
  j+="\"hrSource\":\""+selectedHRSource+"\",";
  j+="\"heartRateValid\":"+String(hrValid?"true":"false")+",";
  j+="\"spo2\":"+String(spo2)+",";
  j+="\"spo2Valid\":"+String(spo2Valid?"true":"false")+",";
  j+="\"ecg\":"+String(ecgValue)+",";
  j+="\"latitude\":"+String(latitude,6)+",";
  j+="\"longitude\":"+String(longitude,6)+",";
  j+="\"gpsValid\":"+String(gpsValid?"true":"false")+",";
  j+="\"tempAbnormal\":"+String(tempAbnormal?"true":"false")+",";
  j+="\"pressureAbnormal\":"+String(pressureAbnormal?"true":"false")+",";
  j+="\"hrPotAbnormal\":"+String(hrPotAbnormal?"true":"false")+",";
  j+="\"maxAbnormal\":"+String(maxAbnormal?"true":"false")+",";
  j+="\"mpuAbnormal\":"+String(mpuAbnormal?"true":"false")+",";
  j+="\"allAbnormal\":"+String(allAbnormal?"true":"false")+",";
  j+="\"fallDetected\":"+String(fallDetected?"true":"false")+",";
  j+="\"state\":\""+stateName()+"\",";
  j+="\"reason\":\""+jsonEscape(alertReason)+"\"}";
  return j;
}

void updateOLED(){
  oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0,9,"MEDIWATCH NEO");oled.drawStr(91,9,stateName());
  char b[32];snprintf(b,sizeof(b),"HR: %d %s",effectiveHR,selectedHRSource.c_str());oled.drawStr(0,21,b);
  snprintf(b,sizeof(b),"SpO2: %s",spo2Valid?String(spo2).c_str():"--");oled.drawStr(0,32,b);
  snprintf(b,sizeof(b),"TEMP*: %.1f C",simulatedTemperature);oled.drawStr(0,43,b);
  snprintf(b,sizeof(b),"BP*: %.0f/%.0f",systolic,diastolic);oled.drawStr(0,54,b);
  oled.sendBuffer();
}

void handleRoot(){server.send_P(200,"text/html",PAGE);}
void handleData(){server.send(200,"application/json",jsonData());}
void handleStatus(){
  String j="{";j+="\"mpu6050\":"+String(mpuOK?"true":"false")+",";j+="\"max30102\":"+String(maxOK?"true":"false")+",";j+="\"gps\":"+String(gpsValid?"true":"false")+",";j+="\"gsm\":"+String(gsmOK?"true":"false")+",";j+="\"ad8232\":true,";j+="\"temperaturePot\":true,";j+="\"pressurePot\":true,";j+="\"hrPot\":true,";j+="\"allAbnormal\":"+String(allAbnormal?"true":"false")+"}";server.send(200,"application/json",j);
}

void initSensors(){
  oled.begin();oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(10,25,"MEDIWATCH NEO");oled.drawStr(10,42,"Initialisation...");oled.sendBuffer();
  mpuOK=mpu.begin();
  if(mpuOK){mpu.setAccelerometerRange(MPU6050_RANGE_8_G);mpu.setGyroRange(MPU6050_RANGE_500_DEG);mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);}
  maxOK=max30102.begin(Wire,I2C_SPEED_FAST);
  if(maxOK){max30102.setup(60,4,2,100,411,4096);max30102.setPulseAmplitudeRed(0x24);max30102.setPulseAmplitudeIR(0x24);max30102.clearFIFO();}
  Serial.printf("MPU:%s MAX30102:%s AD8232:OK TEMP-POT:OK PRESSURE-POT:OK HR-POT:OK\n",mpuOK?"OK":"ERR",maxOK?"OK":"ERR");
}

void initWeb(){WiFi.mode(WIFI_AP);WiFi.softAP(WIFI_SSID,WIFI_PASSWORD);server.on("/",HTTP_GET,handleRoot);server.on("/api/data",HTTP_GET,handleData);server.on("/api/status",HTTP_GET,handleStatus);server.begin();Serial.print("Dashboard: http://");Serial.println(WiFi.softAPIP());}

void setup(){
  Serial.begin(115200);delay(500);
  pinMode(SOS_PIN,INPUT_PULLUP);pinMode(BUZZER_PIN,OUTPUT);pinMode(LED_R,OUTPUT);pinMode(LED_G,OUTPUT);pinMode(LED_B,OUTPUT);
  pinMode(ECG_PIN,INPUT);pinMode(TEMP_SIM_PIN,INPUT);pinMode(PRESSURE_PIN,INPUT);pinMode(HR_SIM_PIN,INPUT);
  setLED(false,true,false);stopBuzzer();Wire.begin(I2C_SDA,I2C_SCL);initSensors();
  GPSSerial.begin(GPS_BAUD,SERIAL_8N1,GPS_RX,GPS_TX);GSMSerial.begin(GSM_BAUD,SERIAL_8N1,GSM_RX,GSM_TX);delay(2000);initGSM();initWeb();
  oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(15,20,"MEDIWATCH NEO");oled.drawStr(15,35,"SYSTEM READY");oled.drawStr(15,50,"5/5 ALERT LOGIC");oled.sendBuffer();delay(1000);
}

void loop(){
  unsigned long now=millis();server.handleClient();
  if(now-lastGPS>=20){lastGPS=now;readGPS();}
  if(now-lastTemp>=500){lastTemp=now;readTemperatureSimulation();}
  if(now-lastPressure>=100){lastPressure=now;readPressure();}
  if(now-lastHRSim>=100){lastHRSim=now;readHRSimulation();}
  if(now-lastMPU>=50){lastMPU=now;readMPU();}
  if(now-lastECG>=20){lastECG=now;readECG();}
  readMAX30102();checkSOS();processState();
  if(now-lastDisplay>=500){lastDisplay=now;updateOLED();}
  delay(1);
}
