#include <Arduino.h>
#include <vector>
#include <regex>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <time.h>
#include <TimeLib.h>
#include "driver/adc.h"
#include "esp32/rom/rtc.h"
#include <18B20_class.h>
#include <Adafruit_BME280.h>
#include <WebSocketsServer.h>
#include "SPIFFS.h"
#include <ESPmDNS.h>

#define LOGGER_LOG_MODE  3       // External
#define LOGGER_LOG_LEVEL 4       // Errors & Warnings & Info & Debug & Verbose
bool logToFile = true;
static File logFile;
void _log_printf(const char *format, ...);

#include <ConfigAssist.h>        //Config assist class
#include "user-variables.h"

#define APP_VER "1.2.3"          // Update config assist && move to yaml config

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

#define LAST_BOOT_INI "/lastboot.ini"
#define LAST_BAT_INI   "/batinf.ini"
#define CONNECT_TIMEOUT 8000
#define MAX_SSID_ARR_NO 2 // Maximum ssid json will describe

#define BATT_NO_OF_SAMPLES     8     // Samples to read from BAT_ADC
#define SOIL_NO_OF_SAMPLES     8     // Samples to read from SOIL_PIN

//#define DEBUG_BATTERY              // Uncomment to log bat adc values
#define BATT_CHARGE_DATE_DIVIDER (86400.0F)
#define BATT_PERC_ONPOWER (110.0F)

#define uS_TO_S_FACTOR 1000000ULL     // Conversion factor for micro seconds to seconds
#define SLEEP_CHECK_INTERVAL   1000   // Check if it is time to sleep (millis)
#define SLEEP_DELAY_INTERVAL   30000  // After this time with no activity go to sleep
#define SENSORS_READ_INTERVAL  10000  // Sensors read inverval in milliseconds on loop mode, 30 sec 
#define RESET_CONFIGS_INTERVAL 10000L // Interval press user button to factory defaults.
#define TIME_SYNC_LOOPS  5            // Synchronize time every n loops

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
  // float soilTemp;
  uint8_t soil;
  uint8_t salt;
  // String saltadvice;
  float batPerc;  
  float batVolt;
  uint16_t batAdcVolt;
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
unsigned long sleepTimerCountdown = 2000; // Going to sleep timer
unsigned long sensorReadMs = millis() + SENSORS_READ_INTERVAL;    // Sensors read inteval
unsigned long sleepCheckMs = millis();    // Check for sleep interval
unsigned long btPressMs = 0;              // Button pressed ms

// Bool vars
bool bmeFound = false;
bool dhtFound = false;
bool onPower = false;              // Is battery is charging
bool wifiConnected = false;        // Wifi connected
bool apStarted = false;            // AP started
bool clearMqttRetain = false;
static String chipID;              // Wifi chipid
static String topicsPrefix;        // Subscribe to topics
static uint8_t apClients = 0;      // Connected ap clients

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
BH1750 lightMeter(0x23);          // 0x23
#define LUX_AUTOAJUST             // Auto adjust BH1750 Time register

Adafruit_BME280 *pBmp = NULL;     // 0x77
DHT *pDht=NULL;
DS18B20 *pTemp18B20 = NULL;
WebServer *pServer = NULL;
WebSocketsServer *pWebSocket = NULL;

#ifdef USE_18B20_TEMP_SENSOR 
  DS18B20 temp18B20(DS18B20_PIN);
#endif

// App config 
ConfigAssist conf(CA_DEF_CONF_FILE, VARIABLES_DEF_YAML);   // Config class
ConfigAssist lastBoot(LAST_BOOT_INI);                      // Save last boot vars

#include <files.h>
#include <utils.h>
#include <sleep.h>
#include <battery.h>
#include <sensors.h>
#include <appPMem.h>
#include <chartsPMem.h>
#include <network.h>
#include <mqtt.h>

// Application setup function 
void setup()
{  
  appStart = millis(); //Application start time
 
  //Power control pin, must set high to enable measurements
  pinMode(POWER_CTRL, OUTPUT);
  digitalWrite(POWER_CTRL, 1);
  //Custom button setup
  pinMode(USER_BUTTON, INPUT); 
  delay(100);
  
  chipID = getChipID();
 
  Serial.begin(115200);
  Serial.print("\n\n\n\n");
  Serial.flush();
  //Measure bat with no wifi enabled
  adcVolt =  readBatteryADC();

  //Initiate SPIFFS and Mount file system
  if (!SPIFFS.begin(true)){
    Serial.print("Error mounting SPIFFS\n");
  }
  LOG_I("* * * * Starting v%s * * * * * \n", APP_VER);  

  //Enable configAssist logPrint to file
  logToFile = conf["logFile"].toInt();
  //Failed to load config or ssid empty
  if(!conf.valid() || conf["st_ssid1"]==""){ 
    startAP();
    return;
  }
  
  //Time is ok on wake up but needs to be configured with tz
  setenv("TZ", conf["time_zone"].c_str(), 1);
 
  if(rtc_get_reset_reason(0) == DEEPSLEEP_RESET)  
    LOG_D("Wake up from sleep\n");
  
  //User button check at startup  
  btState = !digitalRead(USER_BUTTON);
  LOG_D("Button: %i, Battery start adc: %lu\n", btState,  adcVolt);
  
  //Free space?
  //listDir("/", 3);
  checkLogRotate();
 
  //Load last boot ini file
  //STORAGE.remove(LAST_BOOT_INI); 
  
  if(!lastBoot.valid()){
    LOG_E("Invalid lastBoot file: %s\n", LAST_BOOT_INI);
    STORAGE.remove(LAST_BOOT_INI);
  } 
     
  //Battery & charging status.
  data.batPerc = truncateFloat(calcBattery(adcVolt),0);

  //Start ST WiFi
  wifiConnected = connectToNetwork();
  //Fall back to AP if connection error and button pressed    
  if(!wifiConnected && !digitalRead(USER_BUTTON) ){    
    startAP();
    return;
  } 
  //Synchronize time if needed or every n loops
  if(getEpoch() <= 10000 || lastBoot["boot_cnt"].toInt() % TIME_SYNC_LOOPS ){
    if(wifiConnected) syncTime();
    else if(getEpoch() > 10000) timeSynchronized = true;
  }else{
    timeSynchronized = true;
  }
  //Initialize on board sensors
  //Wire can not be initialized at beginng, the bus is busy
  if(!initSensors())
    goToDeepSleep("initFail");

  //Log sensors and go to sleep
  if(!wifiConnected) return;
  
  //Connect to mqtt broker
  mqttConnect();
  if(mqttClient.connected())  //Listen config commands
    subscribeConfig();

  //Start web server on long button press or on power connected?
  btState = !digitalRead(USER_BUTTON);
  if(btState){
    startWebSever(); 
    ResetCountdownTimer("Webserver start");
  }
}

// Main application loop
void loop(){
  mqttClient.loop();
  if(pServer) pServer->handleClient();
  if(pWebSocket) pWebSocket->loop();
  //Read and publish sensors  
  if (millis() - sensorReadMs >= SENSORS_READ_INTERVAL){
    //Is time sync
    if(getEpoch() > 10000) timeSynchronized = true;

    //Read sensor values,
    readSensors();    
    
    //Append to log
    logSensors();

    if(!apStarted && !wifiConnected) goToDeepSleep("notConnected");

    //Send measurement to web sockets to update page
    wsSendSensors();

    //Publish JSON to mqtt
    publishSensors(data);
  
    //Remember last volt
    lastBoot.put("bat_voltage", String(data.batVolt, 2),true);
    lastBoot.put("bat_perc", String(data.batPerc, 0),true);
    
    data.sleepReason = "noSleep";
    
    //Read battery status
    adcVolt =  readBatteryADC();

    //Reset loop millis
    sensorReadMs = millis();
  }

  //Check if it is time to sleep
  if (millis() - sleepCheckMs >= SLEEP_CHECK_INTERVAL){
    //Sleep count down
    sleepTimerCountdown = (sleepTimerCountdown > SLEEP_CHECK_INTERVAL) ? (sleepTimerCountdown -= SLEEP_CHECK_INTERVAL) : 0L;
    
    LOG_D("Sleep count down: %lu\n", sleepTimerCountdown);
    
    if(isClientConnected(pServer)){
      //LOG_D("No sleep, clients connected\n");
      ResetCountdownTimer("Clients connected");
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
    //LOG_D("bt: %i\n", btState);
    if(!btPress) btPressMs = millis();
    btPress = true;
    ResetCountdownTimer("Button down");
  }else if(btPress){ //Was pressed and released
    //Update sensors 
    sensorReadMs = millis() + SENSORS_READ_INTERVAL;
    btPress = false;
    LOG_D("Button press for: %lu\n", millis() - btPressMs);
    //Start ap
    if(millis() - btPressMs > RESET_CONFIGS_INTERVAL){
      startAP();
      startWebSever(); 
      ResetCountdownTimer("Long Button down");
      //reset();
      //delay(1009);
      //ESP.restart();
    }
    startWebSever();
  }
  //Clear mqtt retained?
  clearMqttRetainMsg();
  delay(5);
}

//Logger functions
#define MAX_LOG_FMT 128
static char fmtBuf[MAX_LOG_FMT];
static char outBuf[512];
static va_list arglist;

// Custom log print function 
void _log_printf(const char *format, ...){
  strncpy(fmtBuf, format, MAX_LOG_FMT);
  fmtBuf[MAX_LOG_FMT - 1] = 0;
  va_start(arglist, format);  
  vsnprintf(outBuf, MAX_LOG_FMT, fmtBuf, arglist);
  va_end(arglist);
  //size_t msgLen = strlen(outBuf);
  Serial.print(outBuf);
  if (logToFile){
    if(!logFile) logFile = STORAGE.open(LOGGER_LOG_FILENAME, "a+");
    if(!logFile){
      Serial.printf("Failed to open log file: %s\n", LOGGER_LOG_FILENAME);
      logToFile = false;
      return;
    }
    logFile.print(outBuf);
    logFile.flush();
  }
}