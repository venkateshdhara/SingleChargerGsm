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
#define RELAY                                                       6
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
const char BEARER_AIRTEL[] PROGMEM = "airtelgprs.com";
const char BEARER_VI[] PROGMEM ="VI Net Speed";
const char BEARER_BSNL[] PROGMEM ="bsnlnet";


const char URL[] = "http://54.159.4.4:5000/000001/buttonstatus";
const char URLP[] ="http://54.159.4.4:5000/000001/chargerhealth";
const char CONTENT_TYPE[] = "application/json";
char httpData[50];
unsigned long lastmillis = 0,prevtime=0, startmillis=0, endmillis=0, sessionTime=0;
bool timerRun = false, chargerState=false;            
int  rmode, level, pinData=1, blynkUseFlag, blynkBtn=2, prevState=2;
const int nwSelectEEPROMaddress=10, addr = 0;
float w, kWh,vMin, vMax, voltage, temperature;
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
void postdata(float voltage, int level, int status, float acVoltage, float acCurrent, float kwh, float temperature, int time, float units);
void initPostOFF(void);
/*-------------------------------------------------------------------------------------------------------------------------------------------------------------*/
void setup() {  
  pinMode(START_BUTTON, INPUT);
  pinMode(STOP_BUTTON, INPUT);
  pinMode(RELAY, OUTPUT);
  pinMode (FAN_RELAY,OUTPUT);
  dht.begin();
  lcd.init();
  lcd.backlight();
  if(digitalRead(START_BUTTON) == 1  &  digitalRead(STOP_BUTTON) == 1){
    while(digitalRead(START_BUTTON) == 1  &  digitalRead(STOP_BUTTON) == 1);
    while(digitalRead(STOP_BUTTON) == 0){
    int selectionOfNetwork=EEPROM.read(nwSelectEEPROMaddress);
    if(digitalRead(START_BUTTON) == 1){
      selectionOfNetwork++;
      if(selectionOfNetwork >3)
         selectionOfNetwork=0;
      EEPROM.write(nwSelectEEPROMaddress,selectionOfNetwork);  
    if(selectionOfNetwork ==0){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Airtel selected");
    blynkUseFlag =true;
    EEPROM.write(addr, 1);
    }
    else if(selectionOfNetwork ==1){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("VI selected");
    blynkUseFlag =true;
    EEPROM.write(addr, 1);
    }
    else if(selectionOfNetwork ==2){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("BSNL selected");
    blynkUseFlag =true;
    EEPROM.write(addr, 1);
    }
    else if(selectionOfNetwork ==3){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Mannual ");
    blynkUseFlag =false;
    EEPROM.write(addr, 0);
    }
 
      while(digitalRead(START_BUTTON) == 1);
     }
    }
     while(digitalRead(STOP_BUTTON) == 1);
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
  if(blynkUseFlag == true){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting GPRS...");
  int selectionOfNetwork=EEPROM.read(nwSelectEEPROMaddress);
  if(selectionOfNetwork == 0){
    lcd.setCursor(0,1);
    lcd.print("AIRTEL NETWORK");
     result = http.connect(BEARER_AIRTEL);
  }
  else if (selectionOfNetwork ==1)
  {
    lcd.setCursor(0,1);
    lcd.print("VI NETWORK");
     result = http.connect(BEARER_VI);
  }
  else if (selectionOfNetwork ==2)
  {
    lcd.setCursor(0,1);
    lcd.print("BSNL NETWORK");
    result = http.connect(BEARER_BSNL);
  }
  
  delay(5000);
  Serial.print(F("HTTP connect: "));
  Serial.println(result);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("GPRS Connected");

}
  initPostOFF();
  Serial.println("Void setup complete!!!");
 
}
 
void loop() {

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

temperature = readChamberTemperature();
temperature =0;
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
  else{
    voltage =0;
  }
  
  level=mapf(voltage, vMin, vMax, 0, 100);
  if(level < 0)
  level = 0;
lcd.setCursor(0,1);
lcd.print("                ");
lcd.setCursor(0,1);
lcd.print("V:");
lcd.print(voltage,1);
lcd.print("   L:");
lcd.print(level);

if(prevState != blynkBtn){
 if(blynkBtn == 1) {
   prevState =1;
    #if (DEBUG ==1)
    Serial.println("on");
    #endif
    chargerOn();
    startmillis = millis();
    timerRun = true;
    prevtime=millis(); lastmillis=millis();
  }
    if(blynkBtn == 0){
      prevState =0;
    #if (DEBUG == 1)
  Serial.println("off");
  #endif
  chargerOff();
  timerRun = false;
  float power= kWh;
  // lcd.clear();
  // lcd.setCursor(0,0);
  // lcd.print("Power consu:");
  endmillis = millis();
  sessionTime = (endmillis-startmillis)/60000;
  // lcd.print(kWh,3);
  kWh=0;

  prevtime=0; lastmillis=0;
  // delay(10000);
  // lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(2,0);
  lcd.print("CHARGER-OFF");
//while(digitalRead(STOP_BUTTON) == HIGH );
  }
}
if(((millis() - prevtime) >1000) & timerRun){
myTimerEvent();
prevtime=millis();
}

if(blynkUseFlag == true){
postdata(voltage,level,chargerState,emon1.Vrms,emon1.Irms,kWh,temperature,sessionTime,(kWh/1000));
}
}

/*******************************************************************************************************************************************/
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
/*******************************************************************************************************************************************/
int getChargerStatus(void){
  char response[32];
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
// updateVA();

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
  if (t == NAN){
    t=0;
  }
  else if (t == -NAN){
    t=0;
  }
  {
    /* code */
  }
  
  return t;
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------------*/
void postdata(float voltage, int level, int status, float acVoltage, float acCurrent, float kwh, float temperature, int time, float units){
  char p[5];
  dtostrf(voltage,4,1,p);
 int acvol = (int)acVoltage;
    char accurr[5];
  dtostrf(acCurrent,4,1,accurr);
    char ackwh[5];
  dtostrf(kwh,4,1,ackwh);

  char chambtemperature[5];
  dtostrf(temperature,4,1,chambtemperature);

  char acunits[5];
  dtostrf(units,4,1,acunits);
  if(status >= 1)
    status=1;
    char body[120];
    char response[120];
  sprintf(body, "{\"lvl\": %d, \"dcv\": %s, \"stat\":%d, \"acv\":%d, \"aci\":%s, \"kwh\":%s, \"time\":%d, \"t\":%s }", level, p,status,acvol,accurr,ackwh,time,chambtemperature);
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
void initPostOFF(void){
  char msg[]="{\"charger_switch\":0}";
  char response[30];
  http.post(URL, msg, response);
}