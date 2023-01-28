#include <Arduino.h>
#include <sstream>
#include <vector>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <Esp.h>
#include <time.h>
#include <TimeLib.h>
#include "driver/adc.h"
#include <esp_wifi.h>
#include "esp32/rom/rtc.h"
#include <esp_bt.h>
#include "user-variables.h"
#include <18B20_class.h>
#include <Adafruit_BME280.h>
#include <WebServer.h>
#include "SPIFFS.h"
#include <ESPmDNS.h>

#define APP_VER "1.0.0"  // Config with AP portal, sensors calibration, mqtt autodiscovery as device by button in main page

#define LED_PIN 13
#define I2C_SDA 25
#define I2C_SCL 26
#define DHT_PIN 16
#define BAT_ADC 33
#define SALT_PIN 34
#define SOIL_PIN 32
#define BOOT_PIN 0
#define POWER_CTRL 4
#define USER_BUTTON 35
#define DS18B20_PIN 21

// json sensors data 
struct SensorData
{
  String time;
  int bootCnt;
  int bootCntError;
  String sleepReason;
  float lux;
  float temp;
  float humid;
  float soil;
  //float soilTemp;
  float salt;
  //String saltadvice;
  float batPerc;  
  float batVolt;
  uint8_t batAdcVolt;
  String batChargeDate;
  float batDays;
  float pressure;
};

SensorData data;

// Ntp sync?
bool timeSynchronized = false;
unsigned long appStart;

// Reboot counters
RTC_DATA_ATTR int bootCnt = 0;
RTC_DATA_ATTR int bootCntError = 0;
uint16_t adcVolt = 0;
// Timers
unsigned long sleepTimerCountdown = 3000; //Going to sleep timer
unsigned long sensorReadMs = millis() + SENSORS_READ_INTERVAL;    //Sensors read inteval
unsigned long sleepCheckMs = millis();    //Check for sleep interval
// Bool vars
bool bmeFound = false;
bool dhtFound = false;
bool onPower = false;       //Is battery is charging
bool clearMqttRetain = false;
String chipID;              //Wifi chipid
String topicConfig;         //Subscribe config topic
uint8_t apClients = 0;      //Connected ap clients

// User button
bool btPress = false;
bool btState = false;

// Time functions
String timeZone;
String ntpServer;

// Mqtt constants
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Sensors
BH1750 lightMeter(0x23);          //0x23
Adafruit_BME280 *pBmp = NULL;     //0x77
DHT *pDht=NULL;
DS18B20 *pTemp18B20 = NULL;
WebServer *pServer = NULL;

#ifdef USE_18B20_TEMP_SENSOR 
  DS18B20 temp18B20(DS18B20_PIN);
#endif

// Start Subroutines
#include "configAssist.h"        //Setup assistant class
ConfigAssist conf;               //Config class
ConfigAssist lastBoot;           //Save last boot vars

#include <files.h>
#include <utils.h>
#include <sleep.h>
#include <battery.h>
#include <sensors.h>
#include <network.h>
#include <mqtt.h>


// Application setup function 
void setup()
{

  appStart = millis(); //Application start time
  chipID = getChipID();
  //User button check at startup  
  pinMode(USER_BUTTON, INPUT); 
  btState = !digitalRead(USER_BUTTON);
  
  //Sensor power control pin, must set high to enable measurements
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, 1);
  delay(100);

  adcVolt = analogRead(BAT_ADC); //Measure bat with no wifi enabled
  
  Serial.begin(230400);
  Serial.print("\n\n\n\n");
  Serial.flush();
  LOG_INF("Starting..\n");
  LOG_DBG("Button: %i, Battery adc: %lu..\n", btState,  adcVolt);
  
  //Initiate SPIFFS and Mount file system
  if (!SPIFFS.begin(true)){
    LOG_ERR("Error mounting SPIFFS\n");
  }

  //reset();

  int resetReason = rtc_get_reset_reason(0);
  if(resetReason = DEEPSLEEP_RESET){
    LOG_DBG("Wake up from sleep\n");
  }
  
  //Load last boot ini file
  lastBoot.init(lastBootDict_json, LAST_BOOT_CONF);

  //Initialize on board sensors
  initSensors();  
  
  //Initialize config class
  conf.init(appConfigDict_json);
  //Failed to load config or ssid empty
  if(!conf.valid() || conf["st_ssid1"]=="" ){ 
    //Start Access point server and edit config
    pServer = new WebServer(80);
    conf.setup(*pServer, handleAssistRoot);
    ResetCountdownTimer();
    return;
  }
   
  //Start ST WiFi
  connectToNetwork();
  
  //Synchronize time
  syncTime();
  
  btState = !digitalRead(USER_BUTTON);
  //Start web server on long button press or on power connected?
  if((onPower || btState) && pServer==NULL){
    startWebSever(); 
    ResetCountdownTimer();
  }
}

// Main application loop
void loop(){
  mqttClient.loop();
  if(pServer) pServer->handleClient();

  //Read and publish sensors  
  if (millis() - sensorReadMs >= SENSORS_READ_INTERVAL){
    //Read sensor values,
    readSensors();    
    //Create the JSON and publish to mqtt
    publishSensors(data);
  
    //Remember last volt
    lastBoot.put("bat_voltage", String(data.batVolt)); 
    
    //Remember last boot vars?
    lastBoot.saveConfigFile(LAST_BOOT_CONF);

    data.sleepReason = "noSleep";
    
    //Read battery status
    adcVolt = analogRead(BAT_ADC); 
    
    //Reset loop millis
    appStart = millis(); 
    sensorReadMs = millis();
  }

  //Check if it is time to sleep
  if (millis() - sleepCheckMs >= SLEEP_CHECK_INTERVAL){
    //Count down
    sleepTimerCountdown = (sleepTimerCountdown > SLEEP_CHECK_INTERVAL) ? (sleepTimerCountdown -= SLEEP_CHECK_INTERVAL) : 0L;
    LOG_INF("Sleep count down: %lu\n", sleepTimerCountdown);

    if(isClientConnected(pServer)){
      LOG_DBG("No sleep, clients connected\n");
      ResetCountdownTimer();
    }
    //Timeout -> sleep  
    if(sleepTimerCountdown <= 0L ){      
      goToDeepSleep("sleepTimer", false);
    } 

    sleepCheckMs = millis();  
  }
  
  //Check user button
  btState = !digitalRead(USER_BUTTON);
  if(btState){
    btPress = true;
    ResetCountdownTimer();
  }else if(btPress){ //Was pressed and released
    //Update sensors 
    sensorReadMs = millis() + SENSORS_READ_INTERVAL;
    btPress = false;
  }
  delay(5);
}
