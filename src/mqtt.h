

// Handle Mqtt retain commands
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if(length==0) return;
  String msg="";
  for (int i = 0; i < length; i++) {
    msg+= ((char)payload[i]);
  }
  clearMqttRetain = true;
  LOG_I("Mqtt rcv topic: %s, msg: %s\n", topic, msg.c_str());

  //Messages as 'cmd=val'
  char *token = NULL;
  char seps[] = "=";
  token = strtok(&msg[0], seps);
  if(token==NULL){
    LOG_E("Unknown msg: %s \n", msg.c_str());
    return;
  }
  String key(token);

  token = strtok( NULL, seps );
  if(token==NULL){
    LOG_E("Unknown msg: %s \n", msg.c_str());
    return;
  }
  String val(token);
  key.trim();
  val.trim();
  LOG_D("Exec set key: %s=%s\n", key.c_str(), val.c_str());
  if(key=="sleep"){
    if(val=="0"){
      //Don't sleep. Wait for connection
      ResetCountdownTimer("Mqtt sleep off");
      //Start web server if needed
      startWebSever();
    }
  }else if(key=="hasDiscovery"){
      mqttSetupDevice(getChipID());
  }else{
    //Increasments, offs+=1, offs-=1.5
    if(key.endsWith("+") || key.endsWith("-")){
      char sign='+';
      if(key.endsWith("-")) sign = '-';
        key.remove(key.length() - 1);
        key.trim();
        if(!conf.exists(key)){
          LOG_E("Key: %s not exists \n", key.c_str());
          return;
        }

        String kval = conf[key];
        if(!isNumeric(kval)){
          LOG_E("Non numeric: %s\n", kval.c_str());
          return;
        }
        //LOG_D("Inc key: int:%d, %d\n", isInt(kval.c_str()), isInt(val.c_str()));
        //Int
        if(isInt(kval.c_str()) && isInt(val.c_str()) ){
          int i = kval.toInt();
          int inc = val.toInt();
          int n=0;
          if(sign=='-') n = i - inc;
          else n = i + inc;
          LOG_D("Key[%s] inc int: %i %c %i = %i\n", key.c_str(), i, sign, inc, n);
          conf.put(key, String(n) );
        //Float
        }else if(isFloat(kval.c_str()) && isFloat(val.c_str()) ){
          float v = atof(kval.c_str());
          float inc = atof(val.c_str());
          float n;
          if(sign=='-') n = v - inc;
          else n = v + inc;
          LOG_D("Key[%s] float: %3.2f %c %3.2f = %3.2f\n", key.c_str(), v, sign, inc, n);
          conf.put(key, String(n) );
        }else{
          //String inc? //conf.put(key, conf[key].c_str() + val );
          LOG_E("Key %s is not Numeric\n", key.c_str() );
          return;
        }
    }else{
      conf.put(key, val);
      LOG_D("Update key: %s=%s\n", key.c_str(), val.c_str());
    }
    conf.saveConfigFile();
  }
}

// Reset retained messages recv
void clearMqttRetainMsg(){
  if(clearMqttRetain){
    LOG_I("Clear mqtt retaining msgs\n");
    String topic = topicsPrefix + "/config";
    mqttClient.publish(topic.c_str(), "", true);
    mqttClient.loop();
    //delay(1000);
    clearMqttRetain = false;
  }
}

//Send mqtt autodiscovery messages
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

bool retained = true;

if (mqttClient.publish(topic_c, buffer_c, retained)) {
    LOG_D("Message published successfully\n");
}else{
    LOG_E("Error in Message, not published\n");
    goToDeepSleep("mqttPublishFail");
  }
}

// Home Assitant MQTT Autodiscovery messages
// https://www.home-assistant.io/integrations/sensor/#device-class
void mqttSetupDevice(String chipId){
    Serial.println("Setting homeassistant mqtt device..");
    mqttSetup("lux",            chipId, "lx", "illuminance");
    mqttSetup("humid",          chipId, "%",  "humidity");
    if(conf["dht_type"]=="BMP280")
      mqttSetup("press",           chipId, "hPa",  "pressure");
    mqttSetup("soil",           chipId, "%",  "humidity");
    mqttSetup("salt",           chipId, "x");
    mqttSetup("temp",           chipId, "°C", "temperature");
    mqttSetup("batPerc",        chipId, "%",  "battery");
    mqttSetup("batDays",        chipId, "x",  "duration");
    mqttSetup("batVolt",        chipId, "V",  "voltage");
    mqttSetup("RSSI",           chipId, "dBm", "signal_strength");
}

// Receive mqtt config commands
void subscribeConfig(){
  topicsPrefix = conf["mqtt_topic_prefix"] + conf["plant_name"] + "-" + chipID;
  const String topicConf = topicsPrefix + "/config";
  //Subscribe to config topic
  mqttClient.setCallback(mqttCallback);
  mqttClient.subscribe(topicConf.c_str());
  LOG_I("Subscribed at: %s\n",topicConf.c_str());
}

// Get a json with sensors
String getJsonBuff(){

  StaticJsonDocument<1536> doc;
  //Set the values in the document according to SensorData
  JsonObject plant = doc.to<JsonObject>();

  plant["sensorName"] = conf["plant_name"];
  //plant["deviceName"] = conf["host_name"];
  //plant["chipId"] = chipID;
  plant["time"] = data.time;
  plant["lux"] = truncateFloat(data.lux + atof(conf["offs_lux"].c_str()), 1);
  plant["temp"] = truncateFloat(data.temp + atof(conf["offs_temp"].c_str()), 1);
  plant["humid"] = truncateFloat(data.humid + atof(conf["offs_humid"].c_str()), 1);
  if(conf["dht_type"]=="BMP280")
    plant["pressure"] = truncateFloat(data.pressure + atof(conf["offs_pressure"].c_str()), 1);
  plant["soil"] = (data.soil + conf["offs_soil"].toInt());
  plant["salt"] = (data.salt + conf["offs_salt"].toInt());
  // plant["soilTemp"] = config.soilTemp;
  // plant["saltadvice"] = config.saltadvice;
  // plant["plantValveNo"] = plantValveNo;
  // plant["wifissid"] = WiFi.SSID();
  plant["batChargeDate"] = data.batChargeDate;
  plant["batPerc"] = data.batPerc;
#ifdef DEBUG_BATTERY
  plant["batADC"] = data.batAdcVolt;
#endif
  plant["batVolt"] = truncateFloat(data.batVolt, 2);
  plant["batDays"] = truncateFloat(data.batDays, 1);
  //plant["batLastPerc"] = lastBoot["bat_perc"];
  //plant["batPercAvg"] = percAvg;
  //plant["batLastVolt"] = truncate( atof(lastBoot["bat_voltage"].c_str()), 2);
  plant["onPower"] = onPower;
  plant["bootCount"] = data.bootCnt;
  plant["bootCountError"] = data.bootCntError;
  plant["loopMillis"] = millis() - appStart;
  plant["sleepReason"] = data.sleepReason;
  plant["RSSI"] = WiFi.RSSI(); //wifiRSSI;

  #if defined(DEBUG_MQTT)
    serializeJsonPretty(doc, Serial);
    Serial.println();
  #endif
  //Send to mqtt
  char buffer[1536];
  serializeJson(doc, buffer);
  return String(buffer);
}
void mqttConnect(){
  String broker = conf["mqtt_broker"];
  if(broker.length()<1) return;
  int port =  conf["mqtt_port"].toInt();
  //Connect to mqtt broker
  mqttClient.setServer(broker.c_str(), port);
  bool con = mqttClient.connect(broker.c_str(), conf["mqtt_user"].c_str(), conf["mqtt_pass"].c_str());
  if (!con) {
    LOG_E("Connect to MQTT broker: %s:%i FAILED code: %u \n",broker, port, mqttClient.state());
    goToDeepSleep("mqttConnectFail");
  }else{
    LOG_I("Connecting to MQTT broker: %s:%i OK\n", broker, port);
  }
}
// Publish sensors data to mqtt
void publishSensors(const SensorData &data) {
  if(WiFi.status() != WL_CONNECTED) return;

  if(mqttClient.state() != MQTT_CONNECTED)
    mqttConnect();

  if(!mqttClient.connected())  return;
  //Publish status
  topicsPrefix = conf["mqtt_topic_prefix"] + conf["plant_name"] + "-" + chipID;
  const String topicStr =  topicsPrefix + "/status";
  const char* topic = topicStr.c_str();

  String jsonBuff = getJsonBuff();

  LOG_D("Sending message to topic: %s\n",topic);
  #if not defined(DEBUG_MQTT)
    bool retained = true;
    if (mqttClient.publish(topic, jsonBuff.c_str(), retained)) {
      LOG_D("Message published\n");
    } else {
      LOG_E("Error in Message, not published\n");
      goToDeepSleep("mqttPublishFailed");
    }
  #endif
}