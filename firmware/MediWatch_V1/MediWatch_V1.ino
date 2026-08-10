/*
 * MEDIWATCH V1.0
 * ESP32 smart medical-monitoring prototype.
 *
 * V1 architecture: ESP32 + local Wi-Fi dashboard. No Node.js required.
 * WARNING: prototype only; pressure is simulated by potentiometer and
 * fall detection is experimental. Do not use for medical diagnosis.
 */
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <U8g2lib.h>
#include <Adafruit_TMP117.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <MAX30105.h>
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <TinyGPS++.h>

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
#define BUFFER_SIZE 100

const char* AP_SSID = "MEDIWATCH";
const char* AP_PASSWORD = "mediwatch123";

HardwareSerial GPSSerial(1);
HardwareSerial GSMSerial(2);
WebServer server(80);
TinyGPSPlus gps;
Adafruit_TMP117 tmp117;
Adafruit_MPU6050 mpu;
MAX30105 max30102;
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

String patientName="UWASE";
String contact1="+243XXXXXXXXX";
String contact2="+243XXXXXXXXX";
String contact3="+243XXXXXXXXX";

float temperatureC=0;
int heartRate=0, spo2=0;
bool heartRateValid=false, spo2Valid=false;
float pressureSystolic=120, pressureDiastolic=80;
float latitude=0, longitude=0;
bool gpsValid=false;
int ecgValue=0;
float accelX=0,accelY=0,accelZ=0;
bool fallDetected=false;

enum SystemState { STATE_NORMAL, STATE_WARNING, STATE_CRITICAL, STATE_SOS };
SystemState systemState=STATE_NORMAL;
bool alertSent=false;
unsigned long criticalSince=0;
const unsigned long ALERT_CONFIRMATION_TIME=5000;
unsigned long lastDisplay=0,lastTemperature=0,lastMPU=0,lastECG=0,lastPressure=0,lastGPS=0;
const unsigned long DISPLAY_INTERVAL=500,TEMP_INTERVAL=1000,MPU_INTERVAL=100,ECG_INTERVAL=20,PRESSURE_INTERVAL=200,GPS_INTERVAL=50;
uint32_t irBuffer[BUFFER_SIZE],redBuffer[BUFFER_SIZE];
int bufferIndex=0;

void setLED(bool r,bool g,bool b){digitalWrite(LED_R,r);digitalWrite(LED_G,g);digitalWrite(LED_B,b);}
void stopBuzzer(){digitalWrite(BUZZER_PIN,LOW);}
void buzzerShort(){digitalWrite(BUZZER_PIN,HIGH);delay(150);digitalWrite(BUZZER_PIN,LOW);}
void buzzerAlarm(){static unsigned long t=0;static bool s=false;if(millis()-t>=500){t=millis();s=!s;digitalWrite(BUZZER_PIN,s);}}

void oledText(const char* a,const char* b){oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(0,24,a);oled.drawStr(0,40,b);oled.sendBuffer();}

void initSensors(){
  oled.begin(); oledText("MEDIWATCH","Initialisation...");
  if(!tmp117.begin()) Serial.println("TMP117 NOT FOUND"); else Serial.println("TMP117 OK");
  if(!mpu.begin()) Serial.println("MPU6050 NOT FOUND"); else {mpu.setAccelerometerRange(MPU6050_RANGE_8_G);mpu.setGyroRange(MPU6050_RANGE_500_DEG);mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);Serial.println("MPU6050 OK");}
  if(!max30102.begin(Wire,I2C_SPEED_FAST)) Serial.println("MAX30102 NOT FOUND"); else {max30102.setup(60,4,2,100,411,4096);max30102.setPulseAmplitudeRed(0x24);max30102.setPulseAmplitudeIR(0x24);max30102.clearFIFO();Serial.println("MAX30102 OK");}
}

void initGSM(){GSMSerial.println("AT");waitForGSMResponse("OK",2000);GSMSerial.println("ATE0");waitForGSMResponse("OK",2000);GSMSerial.println("AT+CMGF=1");waitForGSMResponse("OK",2000);}

void readTemperature(){sensors_event_t e; if(tmp117.getEvent(&e)) temperatureC=e.temperature;}
void readPressure(){int v=analogRead(PRESSURE_PIN);pressureSystolic=80.0+(v/4095.0)*120.0;pressureDiastolic=pressureSystolic*0.65;}

bool detectFall(){float a=sqrtf(accelX*accelX+accelY*accelY+accelZ*accelZ);return a<2.0f;}
void readMPU(){sensors_event_t a,g,t;if(!mpu.getEvent(&a,&g,&t))return;accelX=a.acceleration.x;accelY=a.acceleration.y;accelZ=a.acceleration.z;if(detectFall())fallDetected=true;}
void readECG(){ecgValue=analogRead(ECG_PIN);}
void readGPS(){while(GPSSerial.available())gps.encode(GPSSerial.read());if(gps.location.isValid()){latitude=gps.location.lat();longitude=gps.location.lng();gpsValid=true;}else gpsValid=false;}

void readMAX30102(){
  max30102.check();
  while(max30102.available()){
    redBuffer[bufferIndex]=max30102.getRed();irBuffer[bufferIndex]=max30102.getIR();max30102.nextSample();
    if(++bufferIndex>=BUFFER_SIZE){int32_t hr,ox;int8_t hrOK,oxOK;maxim_heart_rate_and_oxygen_saturation(irBuffer,BUFFER_SIZE,redBuffer,&ox,&oxOK,&hr,&hrOK);if(hrOK&&hr>0&&hr<250){heartRate=hr;heartRateValid=true;}if(oxOK&&ox>=0&&ox<=100){spo2=ox;spo2Valid=true;}bufferIndex=0;}
  }
}

const char* stateName(){switch(systemState){case STATE_WARNING:return "WARNING";case STATE_CRITICAL:return "CRITICAL";case STATE_SOS:return "SOS";default:return "NORMAL";}}
void processSystemState(){
  bool critical=(pressureSystolic>=160)||(temperatureC>=39)||(spo2Valid&&spo2<=90)||fallDetected;
  bool warning=(pressureSystolic>=140)||(temperatureC>=38)||(spo2Valid&&spo2<=94)||(heartRateValid&&(heartRate<50||heartRate>120));
  if(systemState==STATE_SOS)return;
  if(critical){if(systemState!=STATE_CRITICAL)criticalSince=millis();systemState=STATE_CRITICAL;setLED(true,false,false);buzzerAlarm();if(!alertSent&&millis()-criticalSince>=ALERT_CONFIRMATION_TIME){sendEmergencyAlert("PARAMETRE CRITIQUE");alertSent=true;}}
  else if(warning){systemState=STATE_WARNING;setLED(true,true,false);stopBuzzer();criticalSince=0;}
  else{systemState=STATE_NORMAL;setLED(false,true,false);stopBuzzer();criticalSince=0;alertSent=false;fallDetected=false;}
}

String mapsLink(){if(!gpsValid)return "GPS POSITION UNAVAILABLE";return "https://maps.google.com/?q="+String(latitude,6)+","+String(longitude,6);}
String jsonEscape(String s){s.replace("\\","\\\\");s.replace("\"","\\\"");return s;}
String makeJSON(){String j="{";j+="\"patient\":\""+jsonEscape(patientName)+"\",";j+="\"heartRate\":"+String(heartRate)+",";j+="\"heartRateValid\":"+(heartRateValid?"true":"false")+",";j+="\"spo2\":"+String(spo2)+",";j+="\"spo2Valid\":"+(spo2Valid?"true":"false")+",";j+="\"temperature\":"+String(temperatureC,1)+",";j+="\"systolic\":"+String(pressureSystolic,0)+",";j+="\"diastolic\":"+String(pressureDiastolic,0)+",";j+="\"ecg\":"+String(ecgValue)+",";j+="\"latitude\":"+String(latitude,6)+",";j+="\"longitude\":"+String(longitude,6)+",";j+="\"gpsValid\":"+(gpsValid?"true":"false")+",";j+="\"fallDetected\":"+(fallDetected?"true":"false")+",";j+="\"state\":\""+stateName()+"\"}";return j;}

const char PAGE[] PROGMEM=R"rawliteral(<!doctype html><html lang='fr'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>MediWatch</title><style>body{margin:0;background:#08111f;color:#eaf2ff;font:16px Arial}.w{max-width:1000px;margin:auto;padding:20px}.top{display:flex;justify-content:space-between}.s{padding:8px 12px;border-radius:20px;background:#123d2b;color:#70f0a0}.g{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px}.c{background:#102238;border:1px solid #1e3855;border-radius:16px;padding:18px;margin-top:12px}.v{font-size:30px;font-weight:bold;margin-top:8px}.muted{color:#9bb0c8}.alert{padding:16px;border-radius:14px;background:#123d2b;margin-top:12px}.bad{background:#5b1720}canvas{width:100%;height:180px;background:#071522;border-radius:10px}a{color:#70f0a0}</style></head><body><main class='w'><div class='top'><div><h1>⌚ MEDIWATCH</h1><div class='muted'>Patient: <b id='p'>—</b></div></div><div id='s' class='s'>● CONNEXION</div></div><div class='g'><div class='c'>❤️ FC<div class='v'><span id='hr'>--</span> BPM</div></div><div class='c'>🫁 SpO₂<div class='v'><span id='o'>--</span> %</div></div><div class='c'>🌡️ Temp<div class='v'><span id='t'>--</span> °C</div></div><div class='c'>🩺 Pression<div class='v' id='bp'>--/--</div></div></div><div class='c'>📈 ECG<canvas id='ecg' width='900' height='180'></canvas></div><div class='g'><div class='c'>📍 GPS<div id='gps' class='muted'>Recherche...</div><p><a id='map' target='_blank'>Voir la position</a></p></div><div class='c'>🚨 État<div id='state' class='v'>—</div><div id='fall' class='muted'></div></div></div><div id='a' class='alert'>🟢 Système normal</div></main><script>const $=x=>document.getElementById(x),q=[];let c=$('ecg'),x=c.getContext('2d');function draw(){x.clearRect(0,0,c.width,c.height);x.beginPath();q.forEach((v,i)=>{let X=i*c.width/Math.max(1,q.length-1),Y=90-(v-2048)*.06;i?x.lineTo(X,Y):x.moveTo(X,Y)});x.strokeStyle='#65f3ad';x.stroke()}async function go(){try{let r=await fetch('/api/data',{cache:'no-store'}),d=await r.json();$('p').textContent=d.patient;$('hr').textContent=d.heartRateValid?d.heartRate:'--';$('o').textContent=d.spo2Valid?d.spo2:'--';$('t').textContent=Number(d.temperature).toFixed(1);$('bp').textContent=Math.round(d.systolic)+'/'+Math.round(d.diastolic);$('state').textContent=d.state;$('fall').textContent=d.fallDetected?'⚠️ Chute détectée':'Aucune chute détectée';$('s').textContent='● CONNECTÉ';$('a').textContent=d.state==='CRITICAL'?'🔴 ALERTE CRITIQUE':d.state==='WARNING'?'🟡 ATTENTION':d.state==='SOS'?'🔵 SOS':'🟢 Système normal';$('a').className='alert '+(d.state==='CRITICAL'?'bad':'');if(d.gpsValid){$('gps').textContent=d.latitude.toFixed(6)+', '+d.longitude.toFixed(6);$('map').href='https://maps.google.com/?q='+d.latitude+','+d.longitude}else{$('gps').textContent='GPS indisponible';$('map').removeAttribute('href')}q.push(d.ecg);if(q.length>300)q.shift();draw()}catch(e){$('s').textContent='● HORS LIGNE'}}setInterval(go,500);go();</script></body></html>)rawliteral";

void handleRoot(){server.send_P(200,"text/html",PAGE);}
void handleData(){server.send(200,"application/json",makeJSON());}
void handleStatus(){server.send(200,"application/json",String("{\"state\":\"")+stateName()+"\",\"uptime\":"+String(millis())+"}");}

bool waitForGSMResponse(String expected,unsigned long timeout){unsigned long st=millis();String r="";while(millis()-st<timeout){while(GSMSerial.available()){r+=(char)GSMSerial.read();if(r.indexOf(expected)>=0)return true;}}return false;}
bool sendSMS(String number,String message){if(number.indexOf('X')>=0)return false;GSMSerial.println("AT+CMGF=1");if(!waitForGSMResponse("OK",3000))return false;GSMSerial.print("AT+CMGS=\"");GSMSerial.print(number);GSMSerial.println("\"");if(!waitForGSMResponse(">",5000))return false;GSMSerial.print(message);GSMSerial.write(26);return waitForGSMResponse("OK",15000);}
void sendEmergencyAlert(String reason){String m="MEDIWATCH - ALERTE MEDICALE\nPatient: "+patientName+"\nMotif: "+reason+"\nPression: "+String(pressureSystolic,0)+"/"+String(pressureDiastolic,0)+" mmHg\nFC: "+String(heartRate)+" BPM\nSpO2: "+String(spo2)+" %\nTemperature: "+String(temperatureC,1)+" C\nLOCALISATION: "+mapsLink();Serial.println(m);sendSMS(contact1,m);delay(1000);sendSMS(contact2,m);delay(1000);sendSMS(contact3,m);}

void checkSOS(){static bool old=HIGH;static unsigned long last=0;bool now=digitalRead(SOS_PIN);if(old==HIGH&&now==LOW&&millis()-last>3000){last=millis();systemState=STATE_SOS;setLED(false,false,true);buzzerShort();sendEmergencyAlert("SOS MANUEL");alertSent=true;}old=now;}
void updateOLED(){oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(0,9,"MEDIWATCH");oled.drawStr(90,9,stateName());char b[32];snprintf(b,sizeof(b),"HR: %d BPM",heartRate);oled.drawStr(0,23,b);snprintf(b,sizeof(b),"SpO2: %d %%",spo2);oled.drawStr(0,34,b);snprintf(b,sizeof(b),"TEMP: %.1f C",temperatureC);oled.drawStr(0,45,b);snprintf(b,sizeof(b),"BP: %.0f/%.0f",pressureSystolic,pressureDiastolic);oled.drawStr(0,56,b);oled.sendBuffer();}

void setup(){Serial.begin(115200);pinMode(SOS_PIN,INPUT_PULLUP);pinMode(BUZZER_PIN,OUTPUT);pinMode(LED_R,OUTPUT);pinMode(LED_G,OUTPUT);pinMode(LED_B,OUTPUT);pinMode(ECG_PIN,INPUT);pinMode(PRESSURE_PIN,INPUT);setLED(false,true,false);stopBuzzer();Wire.begin(I2C_SDA,I2C_SCL);initSensors();GPSSerial.begin(GPS_BAUD,SERIAL_8N1,GPS_RX,GPS_TX);GSMSerial.begin(GSM_BAUD,SERIAL_8N1,GSM_RX,GSM_TX);delay(3000);initGSM();WiFi.mode(WIFI_AP);WiFi.softAP(AP_SSID,AP_PASSWORD);server.on("/",handleRoot);server.on("/api/data",handleData);server.on("/api/status",handleStatus);server.begin();Serial.print("Dashboard: http://");Serial.println(WiFi.softAPIP());oledText("MEDIWATCH READY","192.168.4.1");}

void loop(){unsigned long now=millis();server.handleClient();if(now-lastGPS>=GPS_INTERVAL){lastGPS=now;readGPS();}if(now-lastTemperature>=TEMP_INTERVAL){lastTemperature=now;readTemperature();}if(now-lastPressure>=PRESSURE_INTERVAL){lastPressure=now;readPressure();}if(now-lastMPU>=MPU_INTERVAL){lastMPU=now;readMPU();}if(now-lastECG>=ECG_INTERVAL){lastECG=now;readECG();}readMAX30102();checkSOS();processSystemState();if(now-lastDisplay>=DISPLAY_INTERVAL){lastDisplay=now;updateOLED();}}
