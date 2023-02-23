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
#include <18B20_class.h>
#include <Adafruit_BME280.h>
#include <WiFiClientSecure.h>
#include <WebSocketsServer.h>
#include <WebServer.h>
#include "SPIFFS.h"
#include <ESPmDNS.h>
#include "user-variables.h"

#define APP_VER "1.0.7a"  // Generate log file to debug.View log, reset log
//#define APP_VER "1.0.6" // File system using cards, Update config assist
//#define APP_VER "1.0.5" // User interface using css cards.
//#define APP_VER "1.0.4" // Replace mac id, No sleep onPower and error
//#define APP_VER "1.0.3" // View file system logs, truncate values, Added values units, Rotate log files
//#define APP_VER "1.0.2" // Websockets to update, mqtt command to remote setup.
//#define APP_VER "1.0.1" // Added log sensors to a daily csv file, View the log from main page.
//#define APP_VER "1.0.0" // Config with AP portal, sensors calibration, mqtt autodiscovery as device by button in main page

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

#define LAST_BOOT_CONF "/lastboot.ini"
#define CONNECT_TIMEOUT 10000
#define MAX_SSID_ARR_NO 2 //Maximum ssid json will describe

#define BATT_CHARGE_DATE_DIVIDER (86400.0F)
#define BATT_PERC_ONPOWER (105.0F)

#define uS_TO_S_FACTOR 1000000ULL    //Conversion factor for micro seconds to seconds
#define SLEEP_CHECK_INTERVAL   1000  //Check if it is time to sleep (millis)
#define SLEEP_DELAY_INTERVAL   30000 //After this time with no activity go to sleep
#define SENSORS_READ_INTERVAL  30000 //Sensors read inverval in milliseconds on loop mode, 30 sec 
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
  //float soilTemp;
  uint8_t soil;
  uint8_t salt;
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
unsigned long sleepTimerCountdown = 2000; //Going to sleep timer
unsigned long sensorReadMs = millis() + SENSORS_READ_INTERVAL;    //Sensors read inteval
unsigned long sleepCheckMs = millis();    //Check for sleep interval
unsigned long btPressMs = 0;              //Button pressed ms

// Bool vars
bool bmeFound = false;
bool dhtFound = false;
bool onPower = false;       //Is battery is charging
bool clearMqttRetain = false;
static String chipID;              //Wifi chipid
static String topicsPrefix;        //Subscribe to topics
static uint8_t apClients = 0;      //Connected ap clients

// Log to file
#define LOG_FILENAME "/log"
bool logFile = false;
File dbgLog;                         

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
WebSocketsServer *pWebSocket = NULL;

#ifdef USE_18B20_TEMP_SENSOR 
  DS18B20 temp18B20(DS18B20_PIN);
#endif

// App config 
#include "configAssist.h"        //Setup assistant class
ConfigAssist conf;               //Config class
ConfigAssist lastBoot;           //Save last boot vars

#include <files.h>
#include <utils.h>
#include <sleep.h>
#include <battery.h>
#include <sensors.h>
#include <appPmem.h>
#include <network.h>
#include <mqtt.h>

// Application setup function 
void setup()
{  
  appStart = millis(); //Application start time
  chipID = getChipID();
  
  Serial.begin(230400);
  Serial.print("\n\n\n\n");
  Serial.flush();
  //Initiate SPIFFS and Mount file system
  if (!SPIFFS.begin(true)){
    Serial.print("Error mounting SPIFFS\n");
  }
  
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

  //Time is ok on wake up but needs to be configured with tz
  setenv("TZ", conf["time_zone"].c_str(), 1);
  
  //Enable logging with timestamp
  logFile  = conf["logFile"].toInt();
  if(logFile){    
    dbgLog = STORAGE.open(LOG_FILENAME, FILE_APPEND);
    if( !dbgLog ) {
      LOG_ERR("Failed to open log %s\n", LOG_FILENAME);
      logFile = false;
    }
  } 
  
  LOG_INF("* * * * Starting * * * * * \n");
  //Sensor power control pin, must set high to enable measurements
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, 1);
  delay(200);

  if(rtc_get_reset_reason(0) == DEEPSLEEP_RESET)  
    LOG_DBG("Wake up from sleep\n");

  adcVolt = analogRead(BAT_ADC); //Measure bat with no wifi enabled
  //User button check at startup  
  pinMode(USER_BUTTON, INPUT); 
  btState = !digitalRead(USER_BUTTON);
  LOG_DBG("Button: %i, Battery start adc: %lu\n", btState,  adcVolt);
  
  checkLogRotate();
  //listDir("/", 3);

  //Load last boot ini file
  lastBoot.init(lastBootDict_json, LAST_BOOT_CONF);  
  if(!lastBoot.valid()){
    LOG_ERR("Invalid lastBoot file: %s\n", LAST_BOOT_CONF);
    STORAGE.remove(LAST_BOOT_CONF);
  } 
    
  //Initialize on board sensors
  initSensors();  
  
  //Battery perc, and charging status.
  data.batPerc  = calcBattery(adcVolt);
  
  //Start ST WiFi
  connectToNetwork();

  //Synchronize time
  syncTime();
  
  //Connect to mqtt broker
  mqttConnect();
  
  //Listen config commands
  subscribeConfig();

  btState = !digitalRead(USER_BUTTON);
  //Start web server on long button press or on power connected?
  if(btState){
    startWebSever(); 
    ResetCountdownTimer();
  }
}

// Main application loop
void loop(){
  mqttClient.loop();
  if(pServer) pServer->handleClient();
  if(pWebSocket) pWebSocket->loop();
  //Read and publish sensors  
  if (millis() - sensorReadMs >= SENSORS_READ_INTERVAL){
    //Read sensor values,
    readSensors();    
  
    //Send measurement to web sockets to update page
    wsSendSensors();

    //Publish JSON to mqtt
    publishSensors(data);
  
    //Remember last volt
    lastBoot.put("bat_voltage", String(data.batVolt),true);
    lastBoot.put("bat_perc", String(data.batPerc,1),true);
    
    data.sleepReason = "noSleep";
    
    //Read battery status
    adcVolt = analogRead(BAT_ADC); 
    
    //Reset loop millis
    //appStart = millis(); 
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
    btPressMs = millis();
    btPress = true;
    ResetCountdownTimer();
  }else if(btPress){ //Was pressed and released
    //Update sensors 
    sensorReadMs = millis() + SENSORS_READ_INTERVAL;
    btPress = false;
    LOG_DBG("Button press for: %lu\n", millis() - btPressMs);
    startWebSever();
  }
  //Clear mqtt retained?
  clearMqttRetainMsg();
  delay(5);
}
