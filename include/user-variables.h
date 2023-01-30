// *******************************************************************************************************************************
// START userdefined data
// *******************************************************************************************************************************
#include <Arduino.h>
// Turn logging on/off - turn read logfile on/off, turn delete logfile on/off ---> default is false for all 3, otherwise it can cause battery drainage.
const bool logging = false;

// Uncomment next line to select if 18B20 soil temp sensor available, if available -->> define
//#define USE_18B20_TEMP_SENSOR

// If using the Greenhouse automatic watering repo, then assign a waterValveNo to the plant.
//int plantValveNo = 1;

#define LAST_BOOT_CONF "/lastboot.ini"
#define CONNECT_TIMEOUT 10000
#define MAX_SSID_ARR_NO 2 //Maximum ssid json will describe

#define BATT_CHARGE_DATE_DIVIDER (86400.0F)
#define BATT_PERC_ONPOWER (105.0F)

#define uS_TO_S_FACTOR 1000000ULL    //Conversion factor for micro seconds to seconds
#define SLEEP_CHECK_INTERVAL   2000  //Check if it is time to sleep (millis)
#define SLEEP_DELAY_INTERVAL   30000 //After this time with no activity go to sleep
#define SENSORS_READ_INTERVAL  30000 //Sensors read inverval in milliseconds on loop mode, 30 sec 

// Modify the file with the default params for you application
const char* appConfigDict_json PROGMEM = R"~(
[{
"seperator": "Wifi settings"
},{
     "name": "st_ssid1",
    "label": "Name for WLAN to connect",
  "default": ""  
},{
    "name": "st_pass1",
    "label": "Password for WLAN",
  "default": ""
},{
    "name": "st_ssid2",
    "label": "Name for WLAN",
  "default": ""
},{
     "name": "st_pass2",
    "label": "Password for WLAN",
  "default": ""  
},{
"seperator": "Device settings"
},{
     "name": "host_name",
    "label": "Name your device. {mac} will be replaced with WiFi mac id",
  "default": "T-HIGROW_{mac}"
},{
     "name": "plant_name",
    "label": "Name your plant",
  "default": "Strawberries_01"
},{
     "name": "dht_type",
    "label": "Can be DHT11, DHT12, DHT22 or BME280",
  "options": "'BMP280', 'DHT12', 'DHT21', 'DHT22'",  
  "default": "BME280" 
},{
"seperator": "Sensors calibration"
},{
     "name": "soil_min",
    "label": "Soil Dry condition readings",
  "default": "1535"     
},{
     "name": "soil_max",
    "label": "Soil Wet condition readings",
  "default": "3300"     
},{
     "name": "bat_reading_low",
    "label": "Empty battery volt measured after disconecting from board",
  "default": "3.024"     
},{
     "name": "bat_reading_high",
    "label": "Full battery volt measured after disconecting from board ",
  "default": "4.12"     
},{
"seperator": "Sensors offsets"
},{
     "name": "offs_lux",
    "label": "Adjust luminosity readings",
  "default": "0.0"     
},{
     "name": "offs_temp",
    "label": "Adjust temperature readings",
  "default": "0.0"     
},{
     "name": "offs_humid",
    "label": "Adjust humidity readings",
  "default": "0.0"     
},{
     "name": "offs_soil",
    "label": "Adjust soil humidity readings",
  "default": "0.0"     
},{
     "name": "offs_soilTemp",
    "label": "Adjust soil temperature readings (if enabled)",
  "default": "0.0"     
},{
     "name": "offs_salt",
    "label": "Adjust salt readings",
  "default": "0.0"     
},{
     "name": "offs_pressure",
    "label": "Adjust pressure readings",
  "default": "0.0"     
},{
"seperator": "Mosquitto broker"
},{
     "name": "mqtt_broker",
    "label": "Mosquitto broker (Ip or hostname)",
  "default": ""
},{
     "name": "mqtt_port",
    "label": "Mosquitto broker port",
  "default": ""
},{
     "name": "mqtt_user",
    "label": "Mosquitto broker user",
  "default": ""
},{
     "name": "mqtt_pass",
    "label": "Mosquitto broker password",
  "default": ""
},{
     "name": "mqtt_topic_prefix",
    "label": "Mosquitto topic path prefix",
  "default": "homeassistant/sensor/"
},{
"seperator": "Other settings"
},{
     "name": "time_zone",
    "label": "Needs to be a time zone string<br><small>https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv</small>",
  "default": "EET-2EEST,M3.5.0/3,M10.5.0/4"     
},{
     "name": "ntp_server",
    "label": "Time server to sync time",
  "default": "pool.ntp.org"
},{
     "name": "sleep_time",
    "label": "Time to deep sleep after measurements (Seconds)",
  "default": "300"
},{
     "name": "sleep_time_error",
    "label": "Time to deep sleep if any error Wifi, MQTT connections (Seconds)",
  "default": "300"
}])~";


//Last boot save variables
const char* lastBootDict_json PROGMEM = R"~([{
      "name": "bat_charge_date",
     "label": "Date that the battery is stop charging. (leave blank)",
   "default": ""
  },{
      "name": "bat_voltage",
     "label": "The battery voltage. (leave blank)",
   "default": ""
  },{
      "name": "sleep_reason",
     "label": "Sleep reason. (leave blank)",
   "default": ""
  },{
      "name": "boot_cnt",
     "label": "Sleep count. (leave blank)",
   "default": ""
  },{     
    "name": "boot_cnt_err",
     "label": "Sleep error count. (leave blank)",
   "default": ""
  }])~";