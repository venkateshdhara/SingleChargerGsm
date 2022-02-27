#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "SIM800L.h"
#include <EEPROM.h>
#include <Wire.h> 
#include "EmonLib.h"
#include "DHT.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <Http.h>
/*--------------------------------------------------------------------MACROS----------------------------------------------------------------------------------*/
#define vCalibration                                                150
#define currCalibration                                             8
#define DHTTYPE                                                     DHT11
#define DHTPIN                                                      12 
#define RELAY                                                       10
#define START_BUTTON                                                4
#define STOP_BUTTON                                                 5
#define VADC                                                        A0
#define FAN_RELAY                                                   0
#define RST_PIN                                                     13
#define RX_PIN                                                      9
#define TX_PIN                                                      8

/*---------------------------------------------------------INSTANCES-----------------------------------------------------------------------------------------*/
DHT dht(DHTPIN, DHTTYPE);
EnergyMonitor emon1;
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

/*--------------------------------------------------------VARIABLES-------------------------------------------------------------------------------------------*/
const char BEARER[] PROGMEM = "airtelgprs.com";
const char URL[] = "http://54.159.4.4:5000/000003/buttonstatus";
const char URLP[] ="http://54.159.4.4:5000/000003/chargerhealth";
const char CONTENT_TYPE[] = "application/json";
char httpData[200];
unsigned long lastmillis = 0,prevtime=0, postLastmill=0;
bool timerRun = false, chargerState=false;            
int addr = 0, rmode, level, pinData=1, blynkUseFlag, blynkBtn=2, prevState=2;
float w, kWh,vMin, vMax, voltage;
char response[32];
char body[90];
Result result;
HTTP http(9600, RX_PIN, TX_PIN, RST_PIN);

/*---------------------------------------------------------FUNCTION DECLARATIONS-----------------------------------------------------------------------------*/
void setupModule();
int getChargerStatus(void);
float mapf(float x, float in_min, float in_max, float out_min, float out_max); 
void chargerOff(void);
void chargerOn(void);
void updateVA(void);
void myTimerEvent(void);
float readChamberTemperature(void);
void postdata(float voltage, int level, int status, float acVoltage, float acCurrent, float kwh);
/*-------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void setup() {  
  pinMode(START_BUTTON, INPUT);
  pinMode(STOP_BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode (FAN_RELAY,OUTPUT);
  dht.begin();
  if(digitalRead(START_BUTTON) == 1){
    EEPROM.write(addr,1);
  }

  if(digitalRead(STOP_BUTTON) == 1){
    EEPROM.write(addr,0);
  }
  lcd.init();
  lcd.backlight();
  Serial.begin(115200);
  lcd.setCursor(0,0);
  blynkUseFlag = EEPROM.read(addr);
  lcd.clear();
  lcd.setCursor(5,0);
  lcd.print("HITECH");
  lcd.setCursor(2,1);
  lcd.print("INNOVATIONS");
  delay(1500);
  lcd.clear();
  chargerOff();
  emon1.voltage(1, vCalibration, 1.7);    
  emon1.current(3, currCalibration);
   blynkUseFlag=1;
  if(blynkUseFlag == true){
    lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting GPRS...");
  result = http.connect(BEARER);
  Serial.print(F("HTTP connect: "));
  Serial.println(result);
   lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("GPRS Connected");

}
  
  Serial.println("Void setup complete!!!");
 
}
 
void loop() {
lcd.setCursor(0,1);
lcd.print("V:");
lcd.print(voltage,1);
lcd.print("   L:");
lcd.print(level);
 
 if(blynkUseFlag == true){
 blynkBtn =getChargerStatus();
 Serial.print("Get val:");
 Serial.println(blynkBtn);
 }

 if(blynkUseFlag == false){
   if(digitalRead(START_BUTTON) == HIGH)
    blynkBtn =1;
   else if(digitalRead(STOP_BUTTON) == HIGH)
    blynkBtn =0;
 }

 readChamberTemperature();
 int inpuValue = analogRead(VADC);

 if(inpuValue >= 390 & inpuValue <= 503 ){
    vMin = 42.0; vMax = 54.0;
  voltage=mapf(inpuValue, 390, 503, vMin, vMax);
  }

 else if(inpuValue > 525 & inpuValue < 625 ){
    vMin = 56.0; vMax = 67.0;
  voltage=mapf(inpuValue, 525, 625, vMin, vMax);
  }

  else if(inpuValue > 655 & inpuValue < 800 ){
    vMin = 69.0; vMax = 84.0;
  voltage=mapf(inpuValue, 640, 800, vMin, vMax);
  }
  
  level=mapf(voltage, vMin, vMax, 0, 100);
  if(level < 0)
  level = 0;

if(prevState != blynkBtn){
 if(blynkBtn == 1) {
   prevState =1;
    blynkBtn=2;
    #if (DEBUG ==1)
    Serial.println("on");
    #endif
    chargerOn();
    timerRun = true;
    prevtime=millis(); lastmillis=millis();
  }
    if(blynkBtn == 0){
      prevState =0;
    blynkBtn=2;
    #if (DEBUG == 1)
  Serial.println("off");
  #endif
  chargerOff();
  timerRun = false;
  float power= kWh;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Power consu:");
  lcd.print(kWh,3);
  kWh=0;

  prevtime=0; lastmillis=0;
  delay(10000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(2,0);
  lcd.print("CHARGER-OFF");
//while(digitalRead(STOP_BUTTON) == HIGH );
  }
}
if(blynkUseFlag == true){
postdata(voltage,level,chargerState,emon1.Vrms,emon1.Irms,kWh);
}
}

/*******************************************************************************************************************************************/
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
/*******************************************************************************************************************************************/
int getChargerStatus(void){
 result = http.get(URL, response);
 memcpy(httpData,response,strlen(response));
  int switcher = atoi(&httpData[18]);
  if (switcher == 1){
    Serial.println("LED on");
  }
  else if(switcher == 0){
    Serial.println("LED off");
  }
return switcher;
}
/*------------------------------------------------------------------------------------------------------------------------------*/

void chargerOff(void){
  chargerState =false;
digitalWrite(RELAY,LOW);
Serial.println("off");
lcd.setCursor(0,0);
lcd.print("                ");
lcd.setCursor(2,0);
lcd.print("CHARGER-OFF"); 
}

/*---------------------------------------------------------------------------------------------------------------------------------*/

void chargerOn(void){
  chargerState =true;
digitalWrite(RELAY,HIGH);
Serial.println("on");
lcd.setCursor(0,0);
lcd.print("                ");
lcd.setCursor(2,0);
lcd.print("CHARGER-ON");
}
/*------------------------------------------------------------------------------------------------------------------------------*/

void updateVA(void){

if(emon1.Vrms < 50)
emon1.Vrms = 0;
lcd.clear();
lcd.setCursor(0,0);
lcd.print(emon1.Vrms,0);lcd.print("V");
lcd.setCursor(10,0);
lcd.print(emon1.Irms, 2);lcd.print("A");
lcd.setCursor(0,1);
lcd.print(emon1.apparentPower);
lcd.print("W");
lcd.setCursor(10,1);
lcd.print(kWh, 3);
lcd.print("Kwh");
}
/*------------------------------------------------------------------------------------------------------------------------------*/

void myTimerEvent(void)
{
  emon1.calcVI(20, 200);
  if(emon1.Vrms <50.0){
emon1.Vrms=0;
}
if(emon1.Irms < 0.35){
  emon1.Irms = 0.0;
emon1.apparentPower=0.000000;
}
kWh = kWh + emon1.apparentPower * (millis() - lastmillis) / 3600000000.0;
#if (DEBUG ==1)
Serial.print("Vrms: ");
Serial.print(emon1.Vrms, 2);
Serial.print("V");

Serial.print("\tIrms: ");
Serial.print(emon1.Irms, 4);
Serial.print("A");

Serial.print("\tPower: ");
Serial.print(emon1.apparentPower, 4);
Serial.print("W");

Serial.print("\tkWh: ");
Serial.print(kWh, 5);
Serial.print("kWh");

Serial.print("\ttim: ");
Serial.print((millis() - lastmillis)/1000);
Serial.println("s");
#endif
updateVA();

lastmillis = millis();
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------------*/
float readChamberTemperature(void){

  float t = dht.readTemperature();
  if(t < 25){
    digitalWrite(FAN_RELAY,LOW);
  }
  else if(t > 40) {
    digitalWrite(FAN_RELAY,HIGH);
    chargerOff();
  }
  else if (t > 25 & t < 40){
    digitalWrite(FAN_RELAY,HIGH);
  }
  return t;
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------------*/
void postdata(float voltage, int level, int status, float acVoltage, float acCurrent, float kwh){
  char p[5];
  dtostrf(voltage,4,1,p);
  char acvol[5];
  dtostrf(acVoltage,4,1,acvol);
    char accurr[5];
  dtostrf(acVoltage,4,1,accurr);
    char ackwh[5];
  dtostrf(acVoltage,4,1,ackwh);
  if(status >= 1)
    status=1;
 sprintf(body, "{\"level\": %d, \"voltage\": %s, \"status\":%d }", level, p,status);
  result = http.post(URLP, body, response);
  Serial.println(body);
  Serial.print(F("HTTP POST: "));
  Serial.println(result);
  if (result == SUCCESS)
  {
    Serial.println(response);
    
  }
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------------*/