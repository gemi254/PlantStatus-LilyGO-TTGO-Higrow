#define DEBUG_MQTT    //Uncomment to serial print DBG messages
#if defined(DEBUG_MQTT)
  #undef LOG_DBG
  #define LOG_DBG(format, ...) Serial.printf(DBG_FORMAT(format), ##__VA_ARGS__)
#else
  void printfNull(const char *format, ...) {}
  #define LOG_DBG(format, ...) printfNull(DBG_FORMAT(format), ##__VA_ARGS__)
#endif

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg="";
  for (int i = 0; i < length; i++) {
    msg+= ((char)payload[i]);
  }  
  clearMqttRetain = true;
  LOG_INF("Mqtt rcv topic: %s, msg: %s\n", topic, msg.c_str());
  //Don't sleep
  ResetCountdownTimer();
  //Start web server if needed
  startWebSever();
}

void mqttSetup(String identyfikator, String chipId, String uom = "x", String dc = "x" )
{
  const String topicStr_c = conf["mqtt_topic_prefix"] + conf["plant_name"] + "-" + chipId + "/" + identyfikator +"/config";
  const char* topic_c = topicStr_c.c_str();

  StaticJsonDocument<1536> doc_c;
  JsonObject root = doc_c.to<JsonObject>();

  root["name"] = conf["plant_name"] +" "+ identyfikator;

  if ( dc != "x" ) {
    root["device_class"] = dc;
  }

  root["unique_id"] = chipId +"-"+ identyfikator;
  root["state_topic"] = conf["mqtt_topic_prefix"] + conf["plant_name"] + "-" + chipId  + "/status";
  root["value_template"] = "{{ value_json['" + identyfikator +"'] }}";
  if ( uom != "x" ) {
    root["unit_of_measurement"] = uom;
  }
  
  StaticJsonDocument<256> doc_c0;
  JsonObject dev_root = doc_c0.to<JsonObject>();

  dev_root["identifiers"] = chipId;
  dev_root["manufacturer"] = "LILYGO"; 
  dev_root["model"] = conf["host_name"];
  dev_root["name"] = conf["plant_name"];
  dev_root["sw_version"] = APP_VER;

  root["dev"] = dev_root;
  
  //Send to mqtt
  char buffer_c[1536];
  serializeJson(doc_c, buffer_c);

  //Nice print of configuration mqtt message
  #if defined(DEBUG_MQTT)
      Serial.println("*****************************************" );
      Serial.println(topic_c);
      Serial.print("Sending message to topic: \n");
      serializeJsonPretty(doc_c, Serial);
      Serial.println();
  #endif

bool retained = false;

if (mqttClient.publish(topic_c, buffer_c, retained)) {
    LOG_DBG("Message published successfully\n");
}else{
    LOG_ERR("Error in Message, not published\n");
    goToDeepSleep("mqttPublishFail");
  }
}

void mqttSetupDevice(String chipId){    
    //https://www.home-assistant.io/integrations/sensor/#device-class
    //Home Assitant MQTT Autodiscovery messages
    Serial.println("Setting homeassistant mqtt device..");
    mqttSetup("batPerc",        chipId, "%",  "battery");
    mqttSetup("batDays",        chipId, "x",  "duration");
    mqttSetup("batVolt",        chipId, "V",  "voltage");
    mqttSetup("lux",            chipId, "lx", "illuminance");
    mqttSetup("humid",          chipId, "%",  "humidity");
    mqttSetup("soil",           chipId, "%",  "humidity");
    mqttSetup("salt",           chipId, "x");
    mqttSetup("temp",           chipId, "°C", "temperature");
    mqttSetup("RSSI",           chipId, "dBm", "signal_strength");
}
//Receive mqtt config commands
void subscribeConfig(String topicPref){
  //Subscribe to config topic
  topicConfig = topicPref + "/config";
  mqttClient.setCallback(mqttCallback);
  mqttClient.subscribe(topicConfig.c_str());
  LOG_INF("Subscribed at: %s\n",topicConfig.c_str());
}

String getJsonBuff(){

  StaticJsonDocument<1536> doc;
  //Set the values in the document according to SensorData  
  JsonObject plant = doc.to<JsonObject>();
  float voltAvg = (atof(lastBoot["bat_voltage"].c_str()) + data.batVolt ) / 2.0F;
  LOG_DBG("Battery last volt: %s, cur: %1.4f, avg: %1.4f\n", lastBoot["bat_voltage"].c_str(), data.batVolt, voltAvg);
  
  plant["sensorName"] = conf["plant_name"];
  plant["deviceName"] = conf["host_name"];
  plant["chipId"] = chipID;
  plant["time"] = data.time;
  plant["batChargeDate"] = data.batChargeDate;
  plant["bootCount"] = data.bootCnt;
  plant["bootCountError"] = data.bootCntError;    
  plant["sleepReason"] = data.sleepReason;
  plant["batPerc"] = truncate(data.batPerc, 0);
  plant["batDays"] = truncate(data.batDays, 1);
  plant["batVolt"] = truncate( voltAvg, 2);
  plant["batLastVolt"] = truncate( atof(lastBoot["bat_voltage"].c_str()),1);
  plant["lux"] = truncate(data.lux + atof(conf["offs_lux"].c_str()), 2); 
  plant["temp"] = truncate(data.temp + atof(conf["offs_temp"].c_str()), 2);
  plant["humid"] = truncate(data.humid + atof(conf["offs_humid"].c_str()), 2);
  plant["pressure"] = truncate(data.pressure + atof(conf["offs_pressure"].c_str()), 2);
  plant["soil"] = data.soil + atof(conf["offs_soil"].c_str());
  plant["salt"] = data.salt + atof(conf["offs_salt"].c_str());
  // plant["soilTemp"] = config.soilTemp; 
  // plant["saltadvice"] = config.saltadvice;
  // plant["plantValveNo"] = plantValveNo; 
  // plant["wifissid"] = WiFi.SSID();
  plant["onPower"] = onPower;
  plant["loopMillis"] = millis() - appStart;
  plant["RSSI"] = WiFi.RSSI(); //wifiRSSI;
  
  #if defined(DEBUG_MQTT)
    serializeJsonPretty(doc, Serial);
    Serial.println();
  #endif
  // Send to mqtt
  char buffer[1536];
  serializeJson(doc, buffer);  
  return String(buffer);
}

// Allocate a  JsonDocument
void publishSensors(const SensorData &data) {
  if(WiFi.status() != WL_CONNECTED) return;
  
  LOG_INF("ChipId: %s\n", chipID);
  String broker = conf["mqtt_broker"];  
  int port =  conf["mqtt_port"].toInt();

  //Connect to mqtt broker
  LOG_INF("Connecting to the MQTT broker: %s:%i \n", broker, port);
  mqttClient.setServer(broker.c_str(), port);
  if (!mqttClient.connect(broker.c_str(), conf["mqtt_user"].c_str(), conf["mqtt_pass"].c_str())) {
    LOG_ERR("Connect to the MQTT broker: %s:%i FAILED code: %u \n",broker, port, mqttClient.state());
    goToDeepSleep("mqttConnectFail");
  }else{
    LOG_DBG("Connected to MQTT!\n");
  }

  const String topicPref = conf["mqtt_topic_prefix"] + conf["plant_name"] + "-" + chipID + "";  
  //Subscribe to config config
  subscribeConfig(topicPref); 
  
  //Publish status
  const String topicStr =  topicPref + "/status";
  const char* topic = topicStr.c_str();

  String jsonBuff = getJsonBuff();
  
  LOG_DBG("Sending message to topic: %s\n",topic);  
  
  bool retained = true;  
  if (mqttClient.publish(topic, jsonBuff.c_str(), retained)) {
    LOG_DBG("Message published\n");
  } else {
    LOG_ERR("Error in Message, not published\n");
    goToDeepSleep("mqttPublishFailed");
  }    
}

