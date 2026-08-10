/*
 * MEDIWATCH V1.2 FINAL - ESP32
 *
 * Prototype medical-monitoring bracelet.
 * TMP117      -> temperature
 * MAX30102    -> experimental heart rate + SpO2
 * AD8232      -> ECG analog acquisition
 * MPU6050     -> experimental fall detection
 * Potentiometer -> simulated systolic/diastolic pressure
 * GPS         -> location
 * GSM         -> emergency SMS
 * SH1106      -> local display
 * RGB + buzzer + SOS -> local alerts
 * ESP32 Wi-Fi -> local dashboard/API (no Node.js)
 *
 * IMPORTANT: prototype only. Blood pressure is NOT measured.
 * ECG is not clinically interpreted. SpO2 and fall detection are experimental.
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
#include "heartRate.h"
#include <TinyGPS++.h>
#include <math.h>

// ---------------- PINS ----------------
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

// ---------------- WIFI ----------------
const char *WIFI_SSID = "MEDIWATCH";
const char *WIFI_PASSWORD = "mediwatch123";

// ---------------- PATIENT ----------------
String patientName = "UWASE";
String contact1 = "+243XXXXXXXXX";
String contact2 = "+243XXXXXXXXX";
String contact3 = "+243XXXXXXXXX";

// ---------------- THRESHOLDS ----------------
const float PRESSURE_WARNING = 140.0;
const float PRESSURE_CRITICAL = 160.0;
const float TEMP_WARNING = 38.0;
const float TEMP_CRITICAL = 39.0;
const int SPO2_WARNING = 94;
const int SPO2_CRITICAL = 90;
const int HR_LOW = 50;
const int HR_HIGH = 120;

HardwareSerial GPSSerial(1);
HardwareSerial GSMSerial(2);
WebServer server(80);
TinyGPSPlus gps;
Adafruit_TMP117 tmp117;
Adafruit_MPU6050 mpu;
MAX30105 max30102;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

bool tmpOK=false, mpuOK=false, maxOK=false, gsmOK=false;
bool gpsValid=false, hrValid=false, spo2Valid=false, fallDetected=false;
float temperatureC=0, systolic=120, diastolic=80, latitude=0, longitude=0;
int heartRate=0, spo2=0, ecgValue=0;
float accelX=0, accelY=0, accelZ=0, accelMagnitude=9.81;

// ---------------- STATES ----------------
enum State { NORMAL, WARNING, CRITICAL, SOS };
State state=NORMAL;
String alertReason="Aucune alerte";
bool alertSent=false;
unsigned long criticalSince=0;
unsigned long sosSince=0;
unsigned long lastBeep=0;
bool beepState=false;

// ---------------- TIMERS ----------------
unsigned long tDisplay=0,tTemp=0,tMPU=0,tECG=0,tPressure=0,tGPS=0;
const unsigned long DISPLAY_MS=500,TEMP_MS=1000,MPU_MS=100,ECG_MS=20,PRESSURE_MS=200,GPS_MS=50;
const unsigned long CRITICAL_CONFIRM_MS=5000,SOS_SCREEN_MS=3000;

// ---------------- MAX30102 ----------------
const int N=100;
uint32_t ir[N], red[N];
int sampleIndex=0;

// ---------------- WEB PAGE ----------------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MediWatch</title>
<style>body{margin:0;background:#07111f;color:#eaf2ff;font-family:Arial,sans-serif}.wrap{max-width:1050px;margin:auto;padding:18px}.top{display:flex;justify-content:space-between;align-items:center}.brand{font-size:28px;font-weight:800}.status{padding:8px 12px;border-radius:18px;background:#16452f}.muted{color:#9bb0c8}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px}.card{background:#102238;border:1px solid #213a55;border-radius:16px;padding:16px;margin-top:12px}.label{font-size:12px;color:#9eb3c9}.value{font-size:29px;font-weight:bold;margin-top:8px}.alert{padding:14px;border-radius:14px;background:#16452f;margin-top:12px}.danger{background:#641c26}.ecg{width:100%;height:180px;background:#06101a;border-radius:10px}a{color:#70f0a0}.foot{text-align:center;color:#71869d;font-size:12px;padding:20px}</style></head>
<body><main class="wrap"><div class="top"><div><div class="brand">⌚ MEDIWATCH</div><div class="muted">Patient : <b id="patient">—</b></div></div><div id="conn" class="status">● CONNEXION</div></div>
<div class="grid"><div class="card"><div class="label">❤️ FRÉQUENCE CARDIAQUE</div><div class="value"><span id="hr">--</span> BPM</div></div><div class="card"><div class="label">🫁 SpO₂</div><div class="value"><span id="spo2">--</span> %</div></div><div class="card"><div class="label">🌡️ TEMPÉRATURE</div><div class="value"><span id="temp">--</span> °C</div></div><div class="card"><div class="label">🩺 PRESSION SIMULÉE</div><div class="value" id="bp">--/--</div></div></div>
<div class="card"><div class="label">📈 ECG — acquisition</div><canvas id="ecg" class="ecg" width="1000" height="180"></canvas></div>
<div class="grid"><div class="card"><div class="label">📍 GPS</div><div id="gps" class="muted">Recherche...</div><p><a id="map" target="_blank">Voir sur Google Maps</a></p></div><div class="card"><div class="label">🚨 ÉTAT</div><div id="state" class="value">NORMAL</div><div id="reason" class="muted">Aucune alerte</div><div id="fall" class="muted"></div></div></div>
<div id="alert" class="alert">🟢 Système normal</div><div class="foot">MediWatch V1.2 · Prototype · Ne pas utiliser pour un diagnostic médical</div></main>
<script>
const $=id=>document.getElementById(id),points=[];const canvas=$("ecg"),ctx=canvas.getContext("2d");
function draw(){ctx.clearRect(0,0,canvas.width,canvas.height);ctx.beginPath();points.forEach((v,i)=>{const px=i*canvas.width/Math.max(1,points.length-1),py=90-(v-2048)*.06;i?ctx.lineTo(px,py):ctx.moveTo(px,py)});ctx.strokeStyle="#65f3ad";ctx.lineWidth=2;ctx.stroke()}
async function update(){try{const r=await fetch('/api/data',{cache:'no-store'});const d=await r.json();$("patient").textContent=d.patient;$("hr").textContent=d.heartRateValid?d.heartRate:'--';$("spo2").textContent=d.spo2Valid?d.spo2:'--';$("temp").textContent=Number(d.temperature).toFixed(1);$("bp").textContent=Math.round(d.systolic)+'/'+Math.round(d.diastolic);$("state").textContent=d.state;$("reason").textContent=d.reason;$("fall").textContent=d.fallDetected?'⚠️ Chute détectée':'Aucune chute détectée';$("conn").textContent='● CONNECTÉ';const a=$("alert");a.textContent=d.state==='CRITICAL'?'🔴 ALERTE CRITIQUE':d.state==='WARNING'?'🟡 ATTENTION':d.state==='SOS'?'🔵 SOS':'🟢 Système normal';a.className='alert'+(d.state==='CRITICAL'?' danger':'');if(d.gpsValid){$("gps").textContent=Number(d.latitude).toFixed(6)+', '+Number(d.longitude).toFixed(6);$("map").href='https://maps.google.com/?q='+d.latitude+','+d.longitude}else{$("gps").textContent='Position GPS indisponible';$("map").removeAttribute('href')}points.push(d.ecg);if(points.length>300)points.shift();draw()}catch(e){$("conn").textContent='● HORS LIGNE'}}
update();setInterval(update,500);
</script></body></html>
)HTML";

String stateName(){if(state==WARNING)return "WARNING";if(state==CRITICAL)return "CRITICAL";if(state==SOS)return "SOS";return "NORMAL";}

void setLED(bool r,bool g,bool b){digitalWrite(LED_R,r?HIGH:LOW);digitalWrite(LED_G,g?HIGH:LOW);digitalWrite(LED_B,b?HIGH:LOW);}
void stopBuzzer(){digitalWrite(BUZZER_PIN,LOW);beepState=false;}
void buzzerShort(){digitalWrite(BUZZER_PIN,HIGH);delay(120);digitalWrite(BUZZER_PIN,LOW);}
void buzzerAlarm(){if(millis()-lastBeep>=400){lastBeep=millis();beepState=!beepState;digitalWrite(BUZZER_PIN,beepState);}}

bool waitGSM(const String& expected,unsigned long timeout){unsigned long start=millis();String response;while(millis()-start<timeout){while(GSMSerial.available()){char c=GSMSerial.read();Serial.write(c);response+=c;if(response.indexOf(expected)>=0)return true;}delay(1);}return false;}
void initGSM(){GSMSerial.println("AT");if(!waitGSM("OK",2500)){gsmOK=false;return;}GSMSerial.println("ATE0");waitGSM("OK",2000);GSMSerial.println("AT+CMGF=1");gsmOK=waitGSM("OK",2500);}

bool sendSMS(const String& number,const String& message){if(!gsmOK||number.indexOf('X')>=0)return false;GSMSerial.println("AT+CMGF=1");if(!waitGSM("OK",2500))return false;GSMSerial.print("AT+CMGS=\"");GSMSerial.print(number);GSMSerial.println("\"");if(!waitGSM(">",5000))return false;GSMSerial.print(message);GSMSerial.write(26);return waitGSM("OK",15000);}
String mapsLink(){if(!gpsValid)return "GPS POSITION UNAVAILABLE";return "https://maps.google.com/?q="+String(latitude,6)+","+String(longitude,6);}
void sendEmergencyAlert(const String& reason){String msg="MEDIWATCH - ALERTE MEDICALE\nPatient: "+patientName+"\nMotif: "+reason+"\nPression*: "+String(systolic,0)+"/"+String(diastolic,0)+" mmHg\nFC: "+String(heartRate)+" BPM\nSpO2: "+String(spo2)+" %\nTemperature: "+String(temperatureC,1)+" C\nLOCALISATION:\n"+mapsLink();Serial.println(msg);sendSMS(contact1,msg);delay(500);sendSMS(contact2,msg);delay(500);sendSMS(contact3,msg);}

void initSensors(){oled.begin();oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(10,25,"MEDIWATCH");oled.drawStr(10,42,"Initialisation...");oled.sendBuffer();tmpOK=tmp117.begin();mpuOK=mpu.begin();if(mpuOK){mpu.setAccelerometerRange(MPU6050_RANGE_8_G);mpu.setGyroRange(MPU6050_RANGE_500_DEG);mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);}maxOK=max30102.begin(Wire,I2C_SPEED_FAST);if(maxOK){max30102.setup(60,4,2,100,411,4096);max30102.setPulseAmplitudeRed(0x24);max30102.setPulseAmplitudeIR(0x24);max30102.clearFIFO();}Serial.printf("TMP117:%s MPU:%s MAX:%s\n",tmpOK?"OK":"ERR",mpuOK?"OK":"ERR",maxOK?"OK":"ERR");}

void readTemperature(){if(!tmpOK)return;sensors_event_t e;tmp117.getEvent(&e);temperatureC=e.temperature;}
void readPressure(){int v=analogRead(PRESSURE_PIN);systolic=80.0+120.0*v/4095.0;diastolic=systolic*.65;}
void readMPU(){if(!mpuOK)return;sensors_event_t a,g,t;mpu.getEvent(&a,&g,&t);accelX=a.acceleration.x;accelY=a.acceleration.y;accelZ=a.acceleration.z;accelMagnitude=sqrtf(accelX*accelX+accelY*accelY+accelZ*accelZ);static unsigned long lowSince=0;if(accelMagnitude<2.2){if(!lowSince)lowSince=millis();if(millis()-lowSince>180)fallDetected=true;}else lowSince=0;}
void readECG(){ecgValue=analogRead(ECG_PIN);}
void readGPS(){while(GPSSerial.available())gps.encode(GPSSerial.read());if(gps.location.isValid()&&gps.location.age()<5000){latitude=gps.location.lat();longitude=gps.location.lng();gpsValid=true;}else gpsValid=false;}

void readMAX(){if(!maxOK)return;max30102.check();while(max30102.available()){uint32_t rv=max30102.getRed(),iv=max30102.getIR();red[sampleIndex]=rv;ir[sampleIndex]=iv;max30102.nextSample();sampleIndex++;if(sampleIndex>=N){int validBeats=0;float hrSum=0;for(int i=0;i<N;i++){if(checkForBeat((long)ir[i])){static unsigned long lastBeat=0;unsigned long now=millis();if(lastBeat){float bpm=60.0/((now-lastBeat)/1000.0);if(bpm>40&&bpm<220){hrSum+=bpm;validBeats++;}}lastBeat=now;}}if(validBeats){heartRate=(int)round(hrSum/validBeats);hrValid=true;}uint64_t irSum=0,redSum=0;double irSq=0,redSq=0;for(int i=0;i<N;i++){irSum+=ir[i];redSum+=red[i];irSq+=(double)ir[i]*ir[i];redSq+=(double)red[i]*red[i];}float im=irSum/(float)N,rm=redSum/(float)N;float ia=sqrtf(max(0.0,(float)(irSq/N-im*im)));float ra=sqrtf(max(0.0,(float)(redSq/N-rm*rm)));if(im>10000&&ia>50&&rm>1000){float ratio=(ra/rm)/(ia/im);int estimate=(int)round(110.0-25.0*ratio);estimate=constrain(estimate,70,100);spo2=estimate;spo2Valid=true;}sampleIndex=0;}}}

void processState(){bool critical=false,warning=false;String reason="Aucune alerte";if(systolic>=PRESSURE_CRITICAL){critical=true;reason="Pression critique";}else if(systolic>=PRESSURE_WARNING){warning=true;reason="Pression élevée";}if(temperatureC>=TEMP_CRITICAL){critical=true;reason="Température critique";}else if(temperatureC>=TEMP_WARNING&&!critical){warning=true;if(reason=="Aucune alerte")reason="Température élevée";}if(spo2Valid){if(spo2<=SPO2_CRITICAL){critical=true;reason="SpO2 critique";}else if(spo2<=SPO2_WARNING&&!critical){warning=true;if(reason=="Aucune alerte")reason="SpO2 faible";}}if(hrValid&&(heartRate<HR_LOW||heartRate>HR_HIGH)){warning=true;if(reason=="Aucune alerte")reason="Fréquence cardiaque anormale";}if(fallDetected){critical=true;reason="Chute détectée";}if(state==SOS){setLED(false,false,true);buzzerAlarm();return;}alertReason=reason;if(critical){if(state!=CRITICAL)criticalSince=millis();state=CRITICAL;setLED(true,false,false);buzzerAlarm();if(!alertSent&&millis()-criticalSince>=CRITICAL_CONFIRM_MS){sendEmergencyAlert(alertReason);alertSent=true;}}else if(warning){state=WARNING;setLED(true,true,false);stopBuzzer();criticalSince=0;}else{state=NORMAL;setLED(false,true,false);stopBuzzer();criticalSince=0;alertSent=false;fallDetected=false;}}

void checkSOS(){static bool last=HIGH;static unsigned long lastSOS=0;bool current=digitalRead(SOS_PIN);if(last==HIGH&&current==LOW&&millis()-lastSOS>3000){lastSOS=millis();state=SOS;alertReason="SOS manuel";sosSince=millis();setLED(false,false,true);buzzerShort();sendEmergencyAlert("SOS MANUEL");alertSent=true;}last=current;if(state==SOS&&millis()-sosSince>=SOS_SCREEN_MS)state=CRITICAL;}

String jsonData(){String j="{";j+="\"patient\":\""+patientName+"\",";j+="\"heartRate\":"+String(heartRate)+",\"heartRateValid\":"+(hrValid?"true":"false")+",";j+="\"spo2\":"+String(spo2)+",\"spo2Valid\":"+(spo2Valid?"true":"false")+",";j+="\"temperature\":"+String(temperatureC,1)+",\"systolic\":"+String(systolic,1)+",\"diastolic\":"+String(diastolic,1)+",";j+="\"ecg\":"+String(ecgValue)+",\"latitude\":"+String(latitude,6)+",\"longitude\":"+String(longitude,6)+",\"gpsValid\":"+(gpsValid?"true":"false")+",";j+="\"fallDetected\":"+(fallDetected?"true":"false")+",\"state\":\""+stateName()+"\",\"reason\":\""+alertReason+"\"}";return j;}
void handleRoot(){server.send_P(200,"text/html",INDEX_HTML);}void handleData(){server.send(200,"application/json",jsonData());}void handleStatus(){String j="{\"tmp117\":"+(tmpOK?"true":"false")+",\"mpu6050\":"+(mpuOK?"true":"false")+",\"max30102\":"+(maxOK?"true":"false")+",\"gsm\":"+(gsmOK?"true":"false")+",\"gps\":"+(gpsValid?"true":"false")+"}";server.send(200,"application/json",j);}
void initWeb(){WiFi.mode(WIFI_AP);WiFi.softAP(WIFI_SSID,WIFI_PASSWORD);server.on("/",HTTP_GET,handleRoot);server.on("/api/data",HTTP_GET,handleData);server.on("/api/status",HTTP_GET,handleStatus);server.onNotFound([](){server.send(404,"text/plain","Not found");});server.begin();Serial.print("Dashboard: http://");Serial.println(WiFi.softAPIP());}

void setup(){Serial.begin(115200);delay(500);pinMode(SOS_PIN,INPUT_PULLUP);pinMode(BUZZER_PIN,OUTPUT);pinMode(LED_R,OUTPUT);pinMode(LED_G,OUTPUT);pinMode(LED_B,OUTPUT);pinMode(ECG_PIN,INPUT);pinMode(PRESSURE_PIN,INPUT);setLED(false,true,false);stopBuzzer();Wire.begin(I2C_SDA,I2C_SCL);initSensors();GPSSerial.begin(GPS_BAUD,SERIAL_8N1,GPS_RX,GPS_TX);GSMSerial.begin(GSM_BAUD,SERIAL_8N1,GSM_RX,GSM_TX);delay(2000);initGSM();initWeb();oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(15,20,"MEDIWATCH");oled.drawStr(15,35,"SYSTEM READY");oled.drawStr(15,50,"V1.2 FINAL");oled.sendBuffer();}

void loop(){unsigned long now=millis();server.handleClient();if(now-tGPS>=GPS_MS){tGPS=now;readGPS();}if(now-tTemp>=TEMP_MS){tTemp=now;readTemperature();}if(now-tPressure>=PRESSURE_MS){tPressure=now;readPressure();}if(now-tMPU>=MPU_MS){tMPU=now;readMPU();}if(now-tECG>=ECG_MS){tECG=now;readECG();}readMAX();checkSOS();processState();if(now-tDisplay>=DISPLAY_MS){tDisplay=now;updateOLED();}delay(1);}

void updateOLED(){oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(0,10,"MEDIWATCH");oled.drawStr(88,10,stateName().c_str());char b[32];snprintf(b,sizeof(b),"HR : %s BPM",hrValid?String(heartRate).c_str():"--");oled.drawStr(0,24,b);snprintf(b,sizeof(b),"SpO2: %s %%",spo2Valid?String(spo2).c_str():"--");oled.drawStr(0,35,b);snprintf(b,sizeof(b),"TEMP: %.1f C",temperatureC);oled.drawStr(0,46,b);snprintf(b,sizeof(b),"BP*: %.0f/%.0f",systolic,diastolic);oled.drawStr(0,57,b);oled.sendBuffer();}
