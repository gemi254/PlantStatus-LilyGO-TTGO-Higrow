## Plant Status
Plant Status is a plant monitoring and logging application using <a target="_blank" title="Garden Flowers Temperature Moisture Sensor WiFi Bluetooth Wireless Control Meter" href="https://pt.aliexpress.com/item/32815782900.html">**LILYGO TTGO-T-HIGrow**</a> sensor.
Application supports both **DHT** sensors (DHT11,DHT12,DHT22) or the new **BME280** sensor, the **BH1750** light sensor, 
and the internal **soil moisture** and **soil salt** sensor.

## Main features
+ Configure parameters using an **access point** and **config portal** on startup.
+ Publish sensor values in a **mqtt broker**.
+ **Homeassistant** integration using MQTT **auto discovery** interface.
+ **Log** measurements in a daily **csv file** stored in SPIFFS. **View** or **download** the file in browser.
+ **Sensors offsets** for device calibration.
+ User button single Press -> Take measurement.
+ User button long Press -> Connect to network and show a **web page with measurements** on mobile phone.
+ **Websockets** to auto update sensor values in web page (No refresh).
+ **Auto sleep** after no activity.
+ Battery optimization.
+ Mqtt wakeup function and remote configure functions.
+ No need to compile for each device.

## Install
You can compile project using **platformio** or you can download the compiled firmware from **/firmware** folder 
and upload to you device using **esptool.py** with command..

esptool.py --port COM5 write_flash -fs 1MB -fm dout 0x0 PlantStatus.bin

## Usage
On first boot **Plant Status** will create an access point named **T-HIGROW** and wait for a client connection. Use your mobile phone 
to connect and navigate your browser to **192.168.4.1** to enter device **setup portal**. 
Edit application variables and after making necessary changes press the `Save button` to save settings to spiffs.

Reboot your device by pressing `Reboot` button and on next loop device will wake up, take a measurement, publish it to mqtt and enter deep sleep again.
During sleep if you press the **user button** once, device will wake, publish measurements and enter deep sleep again.

If you press **user button** for long time (>5 sec) device will wake up, publish measurements, start web server and wait 30 seconds 
for a connection from a web browser. Navigate you browser to device ip and see the live measurements.

To reconfigure your device press `configure` button end redirect to configure page. 
If you Auto **Home Assistant** discovery press the`HAS discovery` button and a mqtt auto discovery message will be send to Home assistant. 
Visit HAS devices page to see the new **T-HIGROW** MQTT device.

You can view active daily log file by button `Log` in main page. You can view an old history file with command ..

`http://192.168.4.1/cmd?view=/data/2023-01/2023-01-30.csv` 

*(replace dates as needed)*

You can send remote configuration data via retained messages from a mosquitto broker. Messages will be delivered on next reboot,
alter the configuration and saved to SPIFFS

Fom a mqtt broker server publish the command with the parameter you want to change

``mosquitto_pub -r -h {mqtt_host} -u {mqtt_user} -P {mqtt_pass} -t "homeassistant/sensor/{host_name}-{macid}/config" -m "offs_pressure=-2.42"``
 
 *(replace variables surrounded with {}, as needed)*
 
After disconnecting your browser, **device** will automatically enter to deep sleep again to preserve battery.

<p align="center">
  <img src="images/PlantStatus.png">
</p>
