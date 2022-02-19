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



#define vCalibration                                                150
#define currCalibration                                             8
#define DHTTYPE                                                     DHT11
#define DHTPIN                                                      12 
#define RELAY                                                       10
#define START_BUTTON                                                4
#define STOP_BUTTON                                                 5
#define VADC                                                        A0
#define FAN_RELAY                                                   0
#define RST_PIN 13
#define RX_PIN 9
#define TX_PIN 8

DHT dht(DHTPIN, DHTTYPE);
EnergyMonitor emon1;
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display





const char BEARER[] PROGMEM = "airtelgprs.com";
const char APN[] = "airtelgprs.com";
const char URL[] = "http://54.159.4.4:5000/buttonstatus";
const char CONTENT_TYPE[] = "application/json";
char httpData[200];

unsigned long lastmillis = 0,prevtime=0, postLastmill=0;
bool timerRun = false;            
int addr = 0, vMin, vMax, rmode, voltage, level, pinData=1, checkFlag, blynkBtn=2;
float w, kWh;
char response[32];
  char body[90];
  Result result;
 HTTP http(9600, RX_PIN, TX_PIN, RST_PIN);

void setupModule();
int getChargerStatus(void);
void chargerOff(void);
void chargerOn(void);
void updateVA(void);
void myTimerEvent(void);
float readChamberTemperature(void);
void postdata(int voltage, int level);

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
  checkFlag = EEPROM.read(addr);
  
  
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
  result = http.connect(BEARER);
  Serial.print(F("HTTP connect: "));
  Serial.println(result);
  
  Serial.println("Void setup complete!!!");
}
 
void loop() {
 
 
 blynkBtn =getChargerStatus();
 Serial.print("Get val:");
 Serial.println(blynkBtn);
 readChamberTemperature();
 int inpuValue = analogRead(VADC);

 if(inpuValue > 373 & inpuValue < 501 ){
    vMin = 40; vMax = 54;
  voltage=map(inpuValue, 373, 501, vMin, vMax);
  }

 else if(inpuValue > 519 & inpuValue < 625 ){
    vMin = 56; vMax = 67;
  voltage=map(inpuValue, 519, 625, vMin, vMax);
  }

  else if(inpuValue > 640 & inpuValue < 785 ){
    vMin = 69; vMax = 84;
  voltage=map(inpuValue, 640, 785, vMin, vMax);
  }
  
  level=map(voltage, vMin, vMax, 0, 100);
  if(level < 0)
  level = 0;


 if(blynkBtn ==1) {
    blynkBtn=2;
    #if (DEBUG ==1)
    Serial.println("on");
    #endif
    chargerOn();
    timerRun = true;
    prevtime=millis(); lastmillis=millis();
  }
    if(blynkBtn ==0){
    blynkBtn=2;
    #if (DEBUG ==1)
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

    postdata(voltage,level);

 delay(1000);
}

/*******************************************************************************************************************************************/
int getChargerStatus(void){
 result = http.get("http://54.159.4.4:5000/buttonstatus", response);
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
digitalWrite(RELAY,LOW);
Serial.println("off");
lcd.setCursor(0,0);
lcd.print("                ");
lcd.setCursor(2,0);
lcd.print("CHARGER-OFF"); 
}

/*---------------------------------------------------------------------------------------------------------------------------------*/

void chargerOn(void){
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
void postdata(int voltage, int level){
 sprintf(body, "{\"level\": %d, \"voltage\": %d }", level, voltage);
  result = http.post("http://54.159.4.4:5000/chargerhealth", body, response);
  Serial.print(F("HTTP POST: "));
  Serial.println(result);
  if (result == SUCCESS)
  {
    Serial.println(response);
    
  }
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------------*/