#include <Arduino.h>
#include <vector>
#include <regex>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <NTPClient.h>
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
#include <configAssist.h>        //Config assist class
#include "user-variables.h"

#define DEF_LOG_LEVEL '2' //Errors & Warnings

#define APP_VER "1.1.3"   // Added hostname to  download file name
//#define APP_VER "1.1.2" // Updated configAssist v 2.6.2
//#define APP_VER "1.1.1" // Setup webserver on AP to allow live measurements without internet. Synchronize time on AP mode.
//#define APP_VER "1.1.0" // Save config using javascript async requests. Battery debug calibration
//#define APP_VER "1.0.9" // Auto adjust BH1750 Time register, Log sensors, even on no wifi connection
//#define APP_VER "1.0.8" // Battery prercent fix, Time sync ever 2 loops, charge date on a seperate file
//#define APP_VER "1.0.7" // Generate log file to debug.View log, reset log
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

#define LAST_BOOT_INI "/lastboot.ini"
#define LAST_BAT_INI   "/batinf.ini"
#define CONNECT_TIMEOUT 8000
#define MAX_SSID_ARR_NO 2 //Maximum ssid json will describe

//#define DEBUG_BATTERY              //Uncomment to log bat adc values
#define BATT_CHARGE_DATE_DIVIDER (86400.0F)
#define BATT_PERC_ONPOWER (100.0F)

#define uS_TO_S_FACTOR 1000000ULL     //Conversion factor for micro seconds to seconds
#define SLEEP_CHECK_INTERVAL   1000   //Check if it is time to sleep (millis)
#define SLEEP_DELAY_INTERVAL   30000  //After this time with no activity go to sleep
#define SENSORS_READ_INTERVAL  30000  //Sensors read inverval in milliseconds on loop mode, 30 sec 
#define RESET_CONFIGS_INTERVAL 10000L //Interval press user button to factory defaults.
  
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
unsigned long sleepTimerCountdown = 2000; //Going to sleep timer
unsigned long sensorReadMs = millis() + SENSORS_READ_INTERVAL;    //Sensors read inteval
unsigned long sleepCheckMs = millis();    //Check for sleep interval
unsigned long btPressMs = 0;              //Button pressed ms

// Bool vars
bool bmeFound = false;
bool dhtFound = false;
bool onPower = false;              //Is battery is charging
bool wifiConnected = false;        //Wifi connected
bool apStarted = false;            //AP started
bool clearMqttRetain = false;
static String chipID;              //Wifi chipid
static String topicsPrefix;        //Subscribe to topics
static uint8_t apClients = 0;      //Connected ap clients

// User button
bool btPress = false;
bool btState = false;

// Time functions
String timeZone;
String ntpServer;

//Access configAssist module parameters
extern bool ca_logToFile;
extern byte ca_logLevel;
extern File ca_logFile; 

// Mqtt constants
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Sensors
BH1750 lightMeter(0x23);          //0x23
#define LUX_AUTOAJUST             //Auto adjust BH1750 Time register

Adafruit_BME280 *pBmp = NULL;     //0x77
DHT *pDht=NULL;
DS18B20 *pTemp18B20 = NULL;
WebServer *pServer = NULL;
WebSocketsServer *pWebSocket = NULL;

#ifdef USE_18B20_TEMP_SENSOR 
  DS18B20 temp18B20(DS18B20_PIN);
#endif

// App config 
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
  //Initiate SPIFFS and Mount file system
  if (!SPIFFS.begin(true)){
    Serial.print("Error mounting SPIFFS\n");
  }
  
  //Initialize config class
  conf.initJsonDict(appConfigDict_json);
  //Failed to load config or ssid empty
  if(!conf.valid() || conf["st_ssid1"]=="" ){ 
    //Start Access point server and edit config
    pServer = new WebServer(80);
    //Start ap and register configAssist handlers
    conf.setup(*pServer, true);
    apStarted = true;
    
    //Register app webserver handlers
    registerHandlers();
    
    //Start websockets
    startWebSockets();
    
    ResetCountdownTimer("AP start");
    initSensors();    
    return;
  }
  
  //Time is ok on wake up but needs to be configured with tz
  setenv("TZ", conf["time_zone"].c_str(), 1);
  
  //Enable configAssist logPrint to file
  ca_logToFile = conf["logFile"].toInt();
  
  //Set configAssist log level
  ca_logLevel = DEF_LOG_LEVEL;
  
  LOG_INF("* * * * Starting v%s * * * * * \n", APP_VER);
 
  if(rtc_get_reset_reason(0) == DEEPSLEEP_RESET)  
    LOG_DBG("Wake up from sleep\n");

  //Measure bat with no wifi enabled
  adcVolt = analogRead(BAT_ADC); 
  
  //User button check at startup  
  btState = !digitalRead(USER_BUTTON);
  LOG_DBG("Button: %i, Battery start adc: %lu\n", btState,  adcVolt);
  
  //Free space?
  //listDir("/", 3);
  checkLogRotate();
 
  //Load last boot ini file
  //lastBoot.init(lastBootDict_json, LAST_BOOT_INI); 
  //STORAGE.remove(LAST_BOOT_INI); 
  lastBoot.init(LAST_BOOT_INI);  
  if(!lastBoot.valid()){
    LOG_ERR("Invalid lastBoot file: %s\n", LAST_BOOT_INI);
    STORAGE.remove(LAST_BOOT_INI);
  } 
     
  //Battery & charging status.
  data.batPerc = truncateFloat(calcBattery(adcVolt),0);

  //Start ST WiFi
  wifiConnected = connectToNetwork();    
  
  //Synchronize time if needed or every n loops
  if(getEpoch() <= 10000 || lastBoot["boot_cnt"].toInt() % 3 ){
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
  
  //Listen config commands
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
    adcVolt = analogRead(BAT_ADC); 
    
    //Reset loop millis
    sensorReadMs = millis();
  }

  //Check if it is time to sleep
  if (millis() - sleepCheckMs >= SLEEP_CHECK_INTERVAL){
    //Sleep count down
    sleepTimerCountdown = (sleepTimerCountdown > SLEEP_CHECK_INTERVAL) ? (sleepTimerCountdown -= SLEEP_CHECK_INTERVAL) : 0L;
    
    LOG_DBG("Sleep count down: %lu\n", sleepTimerCountdown);
    
    if(isClientConnected(pServer)){
      //LOG_DBG("No sleep, clients connected\n");
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
    //LOG_DBG("bt: %i\n", btState);
    if(!btPress) btPressMs = millis();
    btPress = true;
    ResetCountdownTimer("Button down");
  }else if(btPress){ //Was pressed and released
    //Update sensors 
    sensorReadMs = millis() + SENSORS_READ_INTERVAL;
    btPress = false;
    LOG_DBG("Button press for: %lu\n", millis() - btPressMs);
    //Factory reset 
    if(millis() - btPressMs > RESET_CONFIGS_INTERVAL){
      reset();
      delay(1009);
      ESP.restart();
    }
    startWebSever();
  }
  //Clear mqtt retained?
  clearMqttRetainMsg();
  delay(5);
}
