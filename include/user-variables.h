// *******************************************************************************************************************************
// START userdefined data
// *******************************************************************************************************************************
#include <Arduino.h>
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
     "name": "logFile",
    "label": "Check to enable logging into a text file",
  "checked": "False"
},{  
"seperator": "Sensors calibration"
},{
     "name": "soil_min",
    "label": "Soil Dry condition readings",
  "default": "1535"},{
     "name": "soil_max",
    "label": "Soil Wet condition readings",
  "default": "3300"
},{
     "name": "bat_reading_low",
    "label": "Empty battery volt measured after disconecting from board",
  "default": "3.024"
},{
     "name": "bat_reading_high",
    "label": "Full battery volt measured after charging and disconecting from board",
  "default": "4.12"
},{
     "name": "bat_adc_low",
    "label": "Empty battery adc reading",
  "default": "1551"
},{
     "name": "bat_adc_high",
    "label": "Full battery vadc reading",
  "default": "2370"
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
    "label": "Mosquitto broker port (1883)",
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
  "default": "EET-2EEST,M3.5.0/3,M10.5.0/4",    
 "datalist": "
'Etc/GMT,GMT0'
'Etc/GMT-0,GMT0'
'Etc/GMT-1,<+01>-1'
'Etc/GMT-2,<+02>-2'
'Etc/GMT-3,<+03>-3'
'Etc/GMT-4,<+04>-4'
'Etc/GMT-5,<+05>-5'
'Etc/GMT-6,<+06>-6'
'Etc/GMT-7,<+07>-7'
'Etc/GMT-8,<+08>-8'
'Etc/GMT-9,<+09>-9'
'Etc/GMT-10,<+10>-10'
'Etc/GMT-11,<+11>-11'
'Etc/GMT-12,<+12>-12'
'Etc/GMT-13,<+13>-13'
'Etc/GMT-14,<+14>-14'
'Etc/GMT0,GMT0'
'Etc/GMT+0,GMT0'
'Etc/GMT+1,<-01>1'
'Etc/GMT+2,<-02>2'
'Etc/GMT+3,<-03>3'
'Etc/GMT+4,<-04>4'
'Etc/GMT+5,<-05>5'
'Etc/GMT+6,<-06>6'
'Etc/GMT+7,<-07>7'
'Etc/GMT+8,<-08>8'
'Etc/GMT+9,<-09>9'
'Etc/GMT+10,<-10>10'
'Etc/GMT+11,<-11>11'
'Etc/GMT+12,<-12>12'"
},{
     "name": "ntp_server",
    "label": "Time server to sync time",
  "default": "pool.ntp.org"
},{
     "name": "sleep_time",
    "label": "Time to deep sleep after measurements (Seconds)",
  "default": "600"
},{
     "name": "sleep_time_error",
    "label": "Time to deep sleep if any error Wifi, MQTT connections (Seconds)",
  "default": "300"
}])~";


//Last boot save variables
const char* lastBootDict_json PROGMEM = R"~([
  {
      "name": "bat_perc",
     "label": "The battery voltage. (leave blank)",
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

