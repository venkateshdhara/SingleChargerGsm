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




#define vCalibration                                                150
#define currCalibration                                             8
#define SIM800_RX_PIN                                               8
#define SIM800_TX_PIN                                               9
#define SIM800_RST_PIN                                              13
#define DHTTYPE                                                     DHT11
#define DHTPIN                                                      12 
#define RELAY                                                       10
#define START_BUTTON                                                4
#define STOP_BUTTON                                                 5
#define VADC                                                        A0
#define FAN_RELAY                                                   0
DHT dht(DHTPIN, DHTTYPE);
EnergyMonitor emon1;
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display




const char APN[] = "airtelgprs.com";
const char URL[] = "http://54.159.4.4:5000/buttonstatus";
const char CONTENT_TYPE[] = "application/json";
char httpData[200];
SIM800L* sim800l;
unsigned long lastmillis = 0,prevtime=0, postLastmill=0;
bool timerRun = false;            
int addr = 0, vMin, vMax, rmode, voltage, level, pinData=1, checkFlag, blynkBtn=2;
float w, kWh;



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
  pinMode(SIM800_RST_PIN,OUTPUT);
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
  Serial.println("Void setup complete!!!");
  lcd.setCursor(0,0);
  checkFlag = EEPROM.read(addr);
  SoftwareSerial* serial = new SoftwareSerial(SIM800_RX_PIN, SIM800_TX_PIN);
  serial->begin(9600);
  sim800l = new SIM800L((Stream *)serial, SIM800_RST_PIN, 200, 512);
  // sim800l = new SIM800L((Stream *)serial, SIM800_RST_PIN, 200, 512, (Stream *)&Serial);
  setupModule();
  bool connected = false;
  for(uint8_t i = 0; i < 5 && !connected; i++) {
    delay(1000);
    connected = sim800l->connectGPRS();
  }
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
    
}
 
void loop() {
 blynkBtn =getChargerStatus();
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
/********************************************************************************************************************************************/
void setupModule() {
    // Wait until the module is ready to accept AT commands
  while(!sim800l->isReady()) {
    Serial.println(F("Problem to initialize AT command, retry in 1 sec"));
    delay(1000);
  }
  Serial.println(F("Setup Complete!"));
 
  // Wait for the GSM signal
  uint8_t signal = sim800l->getSignal();
  while(signal != 0) {
    delay(1000);
    signal = sim800l->getSignal();
  }
  Serial.print(F("Signal OK (strenght: "));
  Serial.print(signal);
  Serial.println(F(")"));
  delay(10);


  // Wait for operator network registration (national or roaming network)
  NetworkRegistration network = sim800l->getRegistrationStatus();
  while(network != REGISTERED_HOME && network != REGISTERED_ROAMING) {
    delay(1000);
    network = sim800l->getRegistrationStatus();
  }
  Serial.println(F("Network registration OK"));
  
  delay(1000);

  // Setup APN for GPRS configuration
  bool success = sim800l->setupGPRS(APN);
  while(!success) {
    success = sim800l->setupGPRS(APN);
    delay(5000);
  }
  Serial.println(F("GPRS config OK"));
 
}
/*******************************************************************************************************************************************/
int getChargerStatus(void){
  Serial.println(F("Start HTTP GET..."));

  // Do HTTP GET communication with 10s for the timeout (read)
  uint16_t rc = sim800l->doGet(URL, 10000);
   if(rc == 200) {
    // Success, output the data received on the serial
    memcpy(httpData,sim800l->getDataReceived(), sim800l->getDataSizeReceived()); 
//     Serial.println(sim800l->getDataReceived());  
  } else {
    // Failed...
    
    Serial.print(F("HTTP GET error "));
    Serial.println(sim800l->getDataReceived());
    Serial.println(rc);
    sim800l->reset();
    setupModule();
    bool connected = false;
    for(uint8_t i = 0; i < 5 && !connected; i++) {
    delay(1000);
    connected = sim800l->connectGPRS();
    }
  }
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
  // char PAYLOAD[100];
  //  char URLP[] = "http://54.159.4.4:5000/chargerhealth";
  //  char CONTENT_TYPE[] = "application/json";
  // char PAYLOAD[] = "{\"voltage\": 60, \"level\": 98}";
  // Serial.println(PAYLOAD);
  // uint16_t rc = sim800l->doPost(URLP, CONTENT_TYPE, PAYLOAD, 512, 512);
}
/*---------------------------------------------------------------------------------------------------------------------------------------------------------*/
