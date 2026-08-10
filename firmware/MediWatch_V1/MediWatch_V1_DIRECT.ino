/* MEDIWATCH V1.2 DIRECT
 * Upload this file directly to an ESP32.
 * Prototype only: BP is simulated; ECG/SpO2/fall detection are not medical-grade.
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

#define SDA_PIN 21
#define SCL_PIN 22
#define ECG_PIN 34
#define BP_PIN 35
#define GPS_RX 16
#define GPS_TX 17
#define GSM_RX 26
#define GSM_TX 27
#define SOS_PIN 32
#define BUZZER 14
#define LED_R 25
#define LED_G 33
#define LED_B 13

const char* AP_NAME="MEDIWATCH";
const char* AP_PASS="mediwatch123";
String patient="UWASE";
String c1="+243XXXXXXXXX",c2="+243XXXXXXXXX",c3="+243XXXXXXXXX";

const float BP_WARN=140,BP_CRIT=160,T_WARN=38,T_CRIT=39;
const int O2_WARN=94,O2_CRIT=90,HR_MIN=50,HR_MAX=120;

HardwareSerial GPS(1),GSM(2);
WebServer web(80);
TinyGPSPlus nav;
Adafruit_TMP117 tmp;
Adafruit_MPU6050 imu;
MAX30105 pulse;
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0,U8X8_PIN_NONE);

bool tmpOK=false,imuOK=false,pulseOK=false,gsmOK=false,gpsOK=false;
bool hrOK=false,o2OK=false,fall=false;
float tempC=0,bpSys=120,bpDia=80,lat=0,lon=0;
int hr=0,o2=0,ecg=0;
float ax=0,ay=0,az=9.81,acc=9.81;

enum Mode{M_NORMAL,M_WARNING,M_CRITICAL,M_SOS};
Mode mode=M_NORMAL;
String reason="Aucune alerte";
unsigned long criticalAt=0,sosAt=0,lastBeep=0,lastDisplay=0,lastTemp=0,lastIMU=0,lastECG=0,lastBP=0,lastGPS=0;
bool beep=false,alertSent=false;

const int SPO2_N=100;
uint32_t ir[SPO2_N],red[SPO2_N];
int n=0;

const char PAGE[] PROGMEM=R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MediWatch</title><style>
body{margin:0;background:#07111f;color:#eaf2ff;font:16px Arial}.w{max-width:1050px;margin:auto;padding:18px}.top{display:flex;justify-content:space-between;align-items:center}.brand{font-size:28px;font-weight:bold}.status,.alert{padding:10px 14px;border-radius:15px;background:#16452f}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(190px,1fr));gap:12px}.card{background:#102238;border:1px solid #213a55;border-radius:16px;padding:16px;margin-top:12px}.label{font-size:12px;color:#9eb3c9}.value{font-size:29px;font-weight:bold;margin-top:7px}.muted{color:#9bb0c8}.bad{background:#641c26}canvas{width:100%;height:180px;background:#06101a;border-radius:10px}a{color:#70f0a0}</style></head><body><main class="w"><div class="top"><div><div class="brand">⌚ MEDIWATCH</div><div class="muted">Patient : <b id="p">—</b></div></div><div id="c" class="status">CONNEXION</div></div><div class="grid"><div class="card"><div class="label">❤️ FC</div><div class="value"><span id="hr">--</span> BPM</div></div><div class="card"><div class="label">🫁 SpO₂</div><div class="value"><span id="o2">--</span> %</div></div><div class="card"><div class="label">🌡️ TEMP</div><div class="value"><span id="t">--</span> °C</div></div><div class="card"><div class="label">🩺 BP*</div><div class="value" id="bp">--/--</div></div></div><div class="card"><div class="label">📈 ECG</div><canvas id="g" width="1000" height="180"></canvas></div><div class="grid"><div class="card"><div class="label">📍 GPS</div><div id="gps" class="muted">Recherche...</div><p><a id="map" target="_blank">Google Maps</a></p></div><div class="card"><div class="label">🚨 ÉTAT</div><div id="state" class="value">NORMAL</div><div id="r" class="muted">Aucune alerte</div></div></div><div id="a" class="alert">🟢 Système normal</div><p class="muted">* Pression simulée par potentiomètre. Prototype uniquement.</p></main><script>
const $=i=>document.getElementById(i),q=[],cv=$("g"),cx=cv.getContext("2d");function draw(){cx.clearRect(0,0,cv.width,cv.height);cx.beginPath();q.forEach((v,i)=>{let X=i*cv.width/Math.max(1,q.length-1),Y=90-(v-2048)*.06;i?cx.lineTo(X,Y):cx.moveTo(X,Y)});cx.strokeStyle="#65f3ad";cx.stroke()}async function u(){try{let d=await(await fetch('/api/data')).json();$("p").textContent=d.patient;$("hr").textContent=d.heartRateValid?d.heartRate:'--';$("o2").textContent=d.spo2Valid?d.spo2:'--';$("t").textContent=Number(d.temperature).toFixed(1);$("bp").textContent=Math.round(d.systolic)+'/'+Math.round(d.diastolic);$("state").textContent=d.state;$("r").textContent=d.reason;$("c").textContent='● CONNECTÉ';let a=$("a");a.textContent=d.state==='CRITICAL'?'🔴 ALERTE CRITIQUE':d.state==='WARNING'?'🟡 ATTENTION':d.state==='SOS'?'🔵 SOS':'🟢 Système normal';a.className='alert '+(d.state==='CRITICAL'?'bad':'');if(d.gpsValid){$("gps").textContent=d.latitude.toFixed(6)+', '+d.longitude.toFixed(6);$("map").href='https://maps.google.com/?q='+d.latitude+','+d.longitude}else{$("gps").textContent='GPS indisponible';$("map").removeAttribute('href')}q.push(d.ecg);if(q.length>300)q.shift();draw()}catch(e){$("c").textContent='● HORS LIGNE'}}u();setInterval(u,500);
</script></body></html>)HTML";

String modeName(){if(mode==M_WARNING)return"WARNING";if(mode==M_CRITICAL)return"CRITICAL";if(mode==M_SOS)return"SOS";return"NORMAL";}
void leds(bool r,bool g,bool b){digitalWrite(LED_R,r);digitalWrite(LED_G,g);digitalWrite(LED_B,b);}void buzzerOff(){digitalWrite(BUZZER,LOW);beep=false;}void buzzerOne(){digitalWrite(BUZZER,HIGH);delay(120);digitalWrite(BUZZER,LOW);}void alarm(){if(millis()-lastBeep>=400){lastBeep=millis();beep=!beep;digitalWrite(BUZZER,beep);}}

bool gsmWait(const String& want,unsigned long ms){unsigned long st=millis();String s;while(millis()-st<ms){while(GSM.available()){char c=GSM.read();Serial.write(c);s+=c;if(s.indexOf(want)>=0)return true;}delay(1);}return false;}
void gsmInit(){GSM.println("AT");if(!gsmWait("OK",2500))return;GSM.println("ATE0");gsmWait("OK",1500);GSM.println("AT+CMGF=1");gsmOK=gsmWait("OK",2500);}
bool sms(const String& num,const String& text){if(!gsmOK||num.indexOf('X')>=0)return false;GSM.println("AT+CMGF=1");if(!gsmWait("OK",2500))return false;GSM.print("AT+CMGS=\"");GSM.print(num);GSM.println("\"");if(!gsmWait(">",5000))return false;GSM.print(text);GSM.write(26);return gsmWait("OK",15000);}
String maps(){return gpsOK?"https://maps.google.com/?q="+String(lat,6)+","+String(lon,6):"GPS POSITION UNAVAILABLE";}
void emergency(const String& why){String text="MEDIWATCH - ALERTE\nPatient: "+patient+"\nMotif: "+why+"\nBP*: "+String(bpSys,0)+"/"+String(bpDia,0)+" mmHg\nFC: "+String(hr)+" BPM\nSpO2: "+String(o2)+" %\nTemp: "+String(tempC,1)+" C\nGPS: "+maps();Serial.println(text);sms(c1,text);delay(500);sms(c2,text);delay(500);sms(c3,text);}

void readTemp(){if(!tmpOK)return;sensors_event_t e;tmp.getEvent(&e);tempC=e.temperature;}
void readBP(){int v=analogRead(BP_PIN);bpSys=80+120.0*v/4095.0;bpDia=bpSys*.65;}
void readIMU(){if(!imuOK)return;sensors_event_t a,g,t;imu.getEvent(&a,&g,&t);ax=a.acceleration.x;ay=a.acceleration.y;az=a.acceleration.z;acc=sqrtf(ax*ax+ay*ay+az*az);static unsigned long low=0;if(acc<2.2){if(!low)low=millis();if(millis()-low>180)fall=true;}else low=0;}
void readECG(){ecg=analogRead(ECG_PIN);}
void readGPS(){while(GPS.available())nav.encode(GPS.read());if(nav.location.isValid()&&nav.location.age()<5000){lat=nav.location.lat();lon=nav.location.lng();gpsOK=true;}else gpsOK=false;}

void readPulse(){if(!pulseOK)return;pulse.check();while(pulse.available()){uint32_t iv=pulse.getIR(),rv=pulse.getRed();ir[n]=iv;red[n]=rv;pulse.nextSample();if(checkForBeat((long)iv)){static unsigned long lastBeat=0;unsigned long now=millis();if(lastBeat){float bpm=60.0/((now-lastBeat)/1000.0);if(bpm>40&&bpm<220){hr=(int)bpm;hrOK=true;}}lastBeat=now;}n++;if(n>=SPO2_N){uint64_t isum=0,rsum=0;double isq=0,rsq=0;for(int i=0;i<SPO2_N;i++){isum+=ir[i];rsum+=red[i];isq+=(double)ir[i]*ir[i];rsq+=(double)red[i]*red[i];}float im=isum/(float)SPO2_N,rm=rsum/(float)SPO2_N;float ia=sqrtf(max(0.0,(float)(isq/SPO2_N-im*im)));float ra=sqrtf(max(0.0,(float)(rsq/SPO2_N-rm*rm)));if(im>10000&&ia>50&&rm>1000){float ratio=(ra/rm)/(ia/im);o2=constrain((int)round(110-25*ratio),70,100);o2OK=true;}n=0;}}}

void analyze(){bool critical=false,warning=false;String why="Aucune alerte";if(bpSys>=BP_CRIT){critical=true;why="Pression critique";}else if(bpSys>=BP_WARN){warning=true;why="Pression élevée";}if(tempC>=T_CRIT){critical=true;why="Température critique";}else if(tempC>=T_WARN&&!critical){warning=true;if(why=="Aucune alerte")why="Température élevée";}if(o2OK){if(o2<=O2_CRIT){critical=true;why="SpO2 critique";}else if(o2<=O2_WARN&&!critical){warning=true;if(why=="Aucune alerte")why="SpO2 faible";}}if(hrOK&&(hr<HR_MIN||hr>HR_MAX)){warning=true;if(why=="Aucune alerte")why="Fréquence cardiaque anormale";}if(fall){critical=true;why="Chute détectée";}if(mode==M_SOS){leds(false,false,true);alarm();return;}reason=why;if(critical){if(mode!=M_CRITICAL)criticalAt=millis();mode=M_CRITICAL;leds(true,false,false);alarm();if(!alertSent&&millis()-criticalAt>=5000){emergency(reason);alertSent=true;}}else if(warning){mode=M_WARNING;leds(true,true,false);buzzerOff();criticalAt=0;}else{mode=M_NORMAL;leds(false,true,false);buzzerOff();criticalAt=0;alertSent=false;fall=false;}}

void sos(){static bool old=HIGH;static unsigned long last=0;bool now=digitalRead(SOS_PIN);if(old==HIGH&&now==LOW&&millis()-last>3000){last=millis();mode=M_SOS;reason="SOS manuel";sosAt=millis();leds(false,false,true);buzzerOne();emergency("SOS MANUEL");alertSent=true;}old=now;if(mode==M_SOS&&millis()-sosAt>=3000)mode=M_CRITICAL;}

String dataJSON(){String j="{";j+="\"patient\":\""+patient+"\",";j+="\"heartRate\":"+String(hr)+",\"heartRateValid\":";j+=hrOK?"true":"false";j+=",\"spo2\":"+String(o2)+",\"spo2Valid\":";j+=o2OK?"true":"false";j+=",\"temperature\":"+String(tempC,1)+",\"systolic\":"+String(bpSys,1)+",\"diastolic\":"+String(bpDia,1)+",\"ecg\":"+String(ecg)+",\"latitude\":"+String(lat,6)+",\"longitude\":"+String(lon,6)+",\"gpsValid\":";j+=gpsOK?"true":"false";j+=",\"fallDetected\":";j+=fall?"true":"false";j+=",\"state\":\""+modeName()+"\",\"reason\":\""+reason+"\"}";return j;}
void root(){web.send_P(200,"text/html",PAGE);}void api(){web.send(200,"application/json",dataJSON());}void status(){String j="{\"tmp117\":";j+=tmpOK?"true":"false";j+=",\"mpu6050\":";j+=imuOK?"true":"false";j+=",\"max30102\":";j+=pulseOK?"true":"false";j+=",\"gps\":";j+=gpsOK?"true":"false";j+=",\"gsm\":";j+=gsmOK?"true":"false";j+="}";web.send(200,"application/json",j);}
void webInit(){WiFi.mode(WIFI_AP);WiFi.softAP(AP_NAME,AP_PASS);web.on("/",HTTP_GET,root);web.on("/api/data",HTTP_GET,api);web.on("/api/status",HTTP_GET,status);web.begin();Serial.print("Dashboard: http://");Serial.println(WiFi.softAPIP());}

void setup(){Serial.begin(115200);pinMode(SOS_PIN,INPUT_PULLUP);pinMode(BUZZER,OUTPUT);pinMode(LED_R,OUTPUT);pinMode(LED_G,OUTPUT);pinMode(LED_B,OUTPUT);pinMode(ECG_PIN,INPUT);pinMode(BP_PIN,INPUT);leds(false,true,false);buzzerOff();Wire.begin(SDA_PIN,SCL_PIN);display.begin();display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(10,25,"MEDIWATCH");display.drawStr(10,42,"Initialisation...");display.sendBuffer();tmpOK=tmp.begin();imuOK=imu.begin();if(imuOK){imu.setAccelerometerRange(MPU6050_RANGE_8_G);imu.setGyroRange(MPU6050_RANGE_500_DEG);imu.setFilterBandwidth(MPU6050_BAND_21_HZ);}pulseOK=pulse.begin(Wire,I2C_SPEED_FAST);if(pulseOK){pulse.setup(60,4,2,100,411,4096);pulse.setPulseAmplitudeRed(0x24);pulse.setPulseAmplitudeIR(0x24);pulse.clearFIFO();}GPS.begin(9600,SERIAL_8N1,GPS_RX,GPS_TX);GSM.begin(9600,SERIAL_8N1,GSM_RX,GSM_TX);delay(2000);gsmInit();webInit();display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(15,20,"MEDIWATCH");display.drawStr(15,35,"SYSTEM READY");display.drawStr(15,50,"V1.2");display.sendBuffer();}

void loop(){unsigned long now=millis();web.handleClient();if(now-lastGPS>=GPS_MS){lastGPS=now;readGPS();}if(now-lastTemp>=TEMP_MS){lastTemp=now;readTemp();}if(now-lastBP>=200){lastBP=now;readBP();}if(now-lastIMU>=100){lastIMU=now;readIMU();}if(now-lastECG>=20){lastECG=now;readECG();}readPulse();sos();analyze();if(now-lastDisplay>=500){lastDisplay=now;display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(0,10,"MEDIWATCH");display.drawStr(90,10,modeName().c_str());char b[30];snprintf(b,sizeof(b),"HR: %s BPM",hrOK?String(hr).c_str():"--");display.drawStr(0,24,b);snprintf(b,sizeof(b),"SpO2: %s %%",o2OK?String(o2).c_str():"--");display.drawStr(0,35,b);snprintf(b,sizeof(b),"TEMP: %.1f C",tempC);display.drawStr(0,46,b);snprintf(b,sizeof(b),"BP*: %.0f/%.0f",bpSys,bpDia);display.drawStr(0,57,b);display.sendBuffer();}delay(1);}
