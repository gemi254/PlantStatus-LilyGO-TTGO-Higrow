# Plant Status with LilyGO TTGO T-HiGrow
## TTGO T-HIGrow 
+ Configure parameters using an access point and config portal on startup.
+ Homeassistant integration using MQTT autodiscovery interface.
+ User button single Press -> Take measurement
+ User button loing Press -> Connect to network and show a web page with measurements on mobile phone
+ Auto sleep after no activity
+ Battery optimization.
+ No need to compile for each device.
+ Mqtt wakeup function.

## Install
Download firmware from /firmware folder and upload to you device using **esptool.py**
esptool.py --port COM5 write_flash -fs 1MB -fm dout 0x0 PlantStatus1.0.0.bin
