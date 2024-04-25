// *******************************************************************************************************************************
// START userdefined data
// *******************************************************************************************************************************
#include <Arduino.h>
// Modify the file with the default params for you application
const char* VARIABLES_DEF_YAML PROGMEM = R"~(
Wifi settings:
  - st_ssid1:
      label: Name for WLAN to connect
      default:
  - st_pass1:
      label: Password for WLAN
      default:
  - st_ip1:
      label: Enter a Static ip setup (ip mask gateway) (192.168.4.2 255.255.255.0 192.168.1.1)
      default:
  - st_ssid2:
      label: Name for WLAN
      default:
  - st_pass2:
      label: Password for WLAN
      default:
  - st_ip2:
      label: Enter a Static ip setup (ip mask gateway) (192.168.4.2 255.255.255.0 192.168.1.1)
      default:

Device settings:
  - host_name:
      label: Name your device. {mac} will be replaced with WiFi mac id
      default: T-HIGROW_{mac}
  - plant_name:
      label: Name your plant
      default: Strawberries_01
  - dht_type:
      label: Can be DHT11, DHT12, DHT22 or BME280
      options: >-
        - BMP280: BMP280
        - DHT11: 11
        - DHT21: 21
        - DHT22: 22
      default: 11
  - logFile:
      label: Check to enable logging into a text file
      checked: False

Sensors calibration:
  - soil_min:
      label: Soil WET condition readings (1256)
      default: 1391
  - soil_max:
      label: Soil DRY condition readings (3216)
      default: 3300
  - auto_adjust_soil:
      label: Auto adjust soil_max, soil_min values
      checked: true
  - bat_reading_low:
      label: Empty battery volt measured after disconecting from board (2.92)
      default: 2.92
  - bat_reading_high:
      label: Full battery volt measured after charging and disconecting from board (4.19)
      default: 4.19
  - bat_adc_low:
      label: Empty battery adc reading (1534)
      default: 1534
  - bat_adc_high:
      label: Full battery vadc reading (float:2280 max: 2644)
      default: 2590

Sensors offsets:
  - offs_lux:
      label: Adjust luminosity readings
      default: 0.0
  - offs_temp:
      label: Adjust temperature readings
      default: 0.0
  - offs_humid:
      label: Adjust humidity readings
      default: 0.0
  - offs_soil:
      label: Adjust soil humidity readings
      default: 0.0
  - offs_soilTemp:
      label: Adjust soil temperature readings (if enabled)
      default: 0.0
  - offs_salt:
      label: Adjust salt readings
      default: 0.0
  - offs_pressure:
      label: Adjust pressure readings
      default: 0.0

Mosquitto broker:
  - mqtt_broker:
      label: Mosquitto broker (Ip or hostname)
        Leave blank to disable
      default:
  - mqtt_port:
      label: Mosquitto broker port (1883)
      default:
  - mqtt_user:
      label: Mosquitto broker user
      default:
  - mqtt_pass:
      label: Mosquitto broker password
      default:
  - mqtt_topic_prefix:
      label: Mosquitto topic path prefix
      default: homeassistant/sensor/

Other settings:
  - time_zone:
      label: Needs to be a time zone string<br><small>https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv</small>
      default: EET-2EEST,M3.5.0/3,M10.5.0/4
      datalist: >-
        - Etc/GMT,GMT0
        - Etc/GMT-0,GMT0
        - Etc/GMT-1,<+01>-1
        - Etc/GMT-2,<+02>-2
        - Etc/GMT-3,<+03>-3
        - Etc/GMT-4,<+04>-4
        - Etc/GMT-5,<+05>-5
        - Etc/GMT-6,<+06>-6
        - Etc/GMT-7,<+07>-7
        - Etc/GMT-8,<+08>-8
        - Etc/GMT-9,<+09>-9
        - Etc/GMT-10,<+10>-10
        - Etc/GMT-11,<+11>-11
        - Etc/GMT-12,<+12>-12
        - Etc/GMT-13,<+13>-13
        - Etc/GMT-14,<+14>-14
        - Etc/GMT0,GMT0
        - Etc/GMT+0,GMT0
        - Etc/GMT+1,<-01>1
        - Etc/GMT+2,<-02>2
        - Etc/GMT+3,<-03>3
        - Etc/GMT+4,<-04>4
        - Etc/GMT+5,<-05>5
        - Etc/GMT+6,<-06>6
        - Etc/GMT+7,<-07>7
        - Etc/GMT+8,<-08>8
        - Etc/GMT+9,<-09>9
        - Etc/GMT+10,<-10>10
        - Etc/GMT+11,<-11>11
        - Etc/GMT+12,<-12>12
  - ntp_server:
      label: Time server to sync time
      default: 83.212.108.245
  - time_sync_loops:
      label: Synchronize time from ntp server every time_sync_loops
      default: 5
  - sleep_time:
      label: Time to deep sleep after measurements (Seconds)
      default: 600
  - sleep_time_error:
      label: Time to deep sleep if any error Wifi, MQTT connections (Seconds)
      default: 300
}])~";