#define QUOTE(val) String(F("\"")) + val + String(F("\""))
#define JSON_TEMPLATE( key, val ) QUOTE(key) + F(": ") + (val)

// Get mqtt base publish path + topic]
String getMqttPath(const String &topic, const String device = "sensor/"){
    String topicStr = conf("mqtt_topic_prefix");
    if(!topicStr.endsWith("/")) topicStr += "/";
    if(device != "") topicStr += device + conf("plant_name") + topic;
    else topicStr += topic;
    return topicStr ;
}
// Get home assistant availability topic
String getMqttHasAvailPath(){  return  getMqttPath(F("/status"),""); }

// Connect to mqtt
void mqttConnect(){
  String broker = conf("mqtt_broker");
  if(broker.length()<1) return;
  int port =  conf("mqtt_port").toInt();
  //Connect to mqtt broker
  mqttClient.setServer(broker.c_str(), port);
  bool con = mqttClient.connect(broker.c_str(), conf("mqtt_user").c_str(), conf("mqtt_pass").c_str());
  if (!con) {
    LOG_E("Connect to MQTT broker: %s:%i FAILED code: %u \n",broker, port, mqttClient.state());
    goToDeepSleep("mqttFail");
  }else{
    LOG_I("Connecting to MQTT broker: %s:%i OK\n", broker, port);
    if( !mqttClient.publish( getMqttPath(F("/lwt")).c_str(), "online", true) ) {
      LOG_E("Failed to send lwt mqtt online message\n");
    }

  }
}
// Disconnect from mqtt
void mqttDisconnect() {
  if( !mqttClient.publish( getMqttPath(F("/lwt")).c_str(), "offline", true ) ) {
    LOG_E("Failed to send lwt mqtt offline message\n");
  }
  mqttClient.unsubscribe(getMqttPath(F("/cmd")).c_str());
  mqttClient.unsubscribe(getMqttHasAvailPath().c_str());
  mqttClient.loop();
  mqttClient.disconnect();
  delay(50);
}
// Publish payload data to mqtt
void mqttPublish(const char* topic, const char* payload, boolean retained){
  if(WiFi.status() != WL_CONNECTED) return;

  if(mqttClient.state() != MQTT_CONNECTED)
    mqttConnect();

  if(!mqttClient.connected())  return;

 //Nice print of configuration mqtt message
  LOG_D("mqtt topic: %s\n", topic);
  LOG_V("mqtt payload: %s\n", payload);

  if( mqttClient.publish( topic, payload, retained ) ){
    LOG_V("Message published\n");
  } else {
    LOG_E("Error in Message, not published\n");
    goToDeepSleep("mqttPublishFailed");
  }
}

// Reset retained messages recv
void clearMqttRetainMsg(){
  if(clearMqttRetain){

    String topic = getMqttPath(F("/cmd"));
    LOG_I("Clear mqtt retaining msg at: %s\n", topic.c_str());
    mqttClient.publish(topic.c_str(), "", true);
    mqttClient.loop();
    //delay(1000);
    clearMqttRetain = false;
  }
}
// Handle Mqtt retain commands
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if(length==0) return;
  String msg="";
  for (int i = 0; i < length; i++) {
    msg+= ((char)payload[i]);
  }
  LOG_I("Mqtt rcv topic: %s, msg: %s\n", topic, msg.c_str());
  //Hasio status online message
  if ((String(topic) == conf("mqtt_topic_prefix") + "status") && msg == "online"){
    mqttSetupDevice();
    return;
  }
  //Messages as 'cmd=val'
  clearMqttRetain = true;
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
    if(val!=""){
      //Don't sleep. Wait for connection
      ResetCountdownTimer("Mqtt sleep off", val.toInt()*1000);
      //Start web server if needed
      startWebSever();
    }
  }else if(key=="hasDiscovery"){
      mqttSetupDevice();
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
          //conf.put(key, String(n) );
          conf[key] = n;
        //Float
        }else if(isFloat(kval.c_str()) && isFloat(val.c_str()) ){
          float v = atof(kval.c_str());
          float inc = atof(val.c_str());
          float n;
          if(sign=='-') n = v - inc;
          else n = v + inc;
          LOG_D("Key[%s] float: %3.2f %c %3.2f = %3.2f\n", key.c_str(), v, sign, inc, n);
          //conf.put(key, String(n) );
          conf[key] = n;
        }else{
          //String inc? //conf.put(key, conf[key].c_str() + val );
          LOG_E("Key %s is not Numeric\n", key.c_str() );
          return;
        }
    }else{
      //conf.put(key, val);
      conf[key] = val;
      LOG_D("Update key: %s=%s\n", key.c_str(), val.c_str());
    }
    conf.saveConfigFile();
  }
}
// Receive mqtt config commands
void mqttSubscribe(){
  String topicCmd = getMqttPath(F("/cmd"));
  //Subscribe to config topic
  mqttClient.setCallback(mqttCallback);
  mqttClient.subscribe(topicCmd.c_str());
  LOG_D("Subscribed at: %s\n",topicCmd.c_str());

  topicCmd  = getMqttPath(F("status"),"");
  LOG_D("Subscribed at: %s\n",topicCmd.c_str());
}
// Get mqqtt json device
String hasioGetDevice(){
    String jDev = F("{\n");
    jDev += "    " +JSON_TEMPLATE(F("name"),  QUOTE(conf("plant_name")) ) + F(",\n");
    jDev += "    " +JSON_TEMPLATE(F("model"), QUOTE(conf("host_name") + "-" + ESP.getChipModel()) ) + F(",\n");
    jDev += "    " +JSON_TEMPLATE(F("connections"), "[[" + QUOTE("mac")  + ", " + QUOTE( WiFi.macAddress() )+ "]]") + F(",\n");
    jDev += "    " +JSON_TEMPLATE(F("sw_version"), QUOTE(APP_VER)) + F(",\n");
    jDev += "    " +JSON_TEMPLATE(F("identifiers"), "[" + QUOTE( ESP.getChipModel() + String("-") + ESP.getChipRevision()) + "]") + F(",\n");
    jDev += "    " +JSON_TEMPLATE(F("configuration_url"), QUOTE("http://" + WiFi.localIP().toString() + "/")) + F(",\n");
    jDev += "    " +JSON_TEMPLATE(F("manufacturer"), QUOTE(F("LilyGO")))+ F("\n");
    jDev += "  }";
    return jDev;
}

//Send mqtt autodiscovery entities message
void hasioSetupEntitie(const String &name, const String &displayName, String uom = "x", String dc = "x", const String stateTopic = F("/sensors"))
{
  JsonDocument doc_c;
  JsonObject root = doc_c.to<JsonObject>();
  root["name"] =  displayName;
  root["unique_id"] = conf("host_name")  + F("_") + getChipID() + F("-") + name;
  //root["object_id"] = conf("host_name") +" "+ name;
  root["state_topic"] = getMqttPath(stateTopic);
  root["value_template"] = "{{ value_json['" + name +"'] }}";
  if(stateTopic==F("/sensors"))
    root["state_class"] = "measurement";

  if(stateTopic == F("/state"))
    root["entity_category"] = F("diagnostic");

  if ( uom != "x" )
    root["unit_of_measurement"] = uom;
  if ( dc != "x" )
    root["device_class"] = dc;

  JsonDocument dev_root;
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(dev_root, hasioGetDevice());
  root["device"] = dev_root;

  //Send to mqtt
  char buffer_c[2048];
  serializeJson(doc_c, buffer_c);
  bool retained = true;
  const String topic = getMqttPath(String("/") + name + F("/config"));
  mqttPublish(topic.c_str(), buffer_c, retained);

}

// Home Assitant MQTT Diagnostics network status
void hasioNetworkStatus(){
  LOG_D("Sending homeassistant mqtt Config Status ..\n");
  const String topic = getMqttPath(F("/network/config")).c_str();

  JsonDocument doc_c;
  JsonObject root = doc_c.to<JsonObject>();

  root["name"] = "Network status";
  root["unique_id"] = conf("host_name")  + F("_") + getChipID() + F("-") +  F("network");
  //root["object_id"] = conf("host_name") +" "+ "network";
  root["entity_category"] = "diagnostic";

  root["icon"] = "mdi:check-network";
  root["retain"] = "true";
  root["state_topic"] = getMqttPath(F("/lwt"));
  JsonDocument dev_root;
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(dev_root, hasioGetDevice());
  root["device"] = dev_root;
  //Send to mqtt
  char buffer_c[512];
  serializeJson(doc_c, buffer_c);

  bool retained = true;
  mqttPublish(topic.c_str(), buffer_c, retained);
}
// Home Assitant MQTT Autodiscovery messages
// https://www.home-assistant.io/integrations/sensor/#device-class
void mqttSetupDevice(){

    //Sensors fields
    hasioSetupEntitie("lux",              "Luminosity", "lx", "illuminance");
    hasioSetupEntitie("humid",            "Humidity", "%",  "humidity");
    if(conf("dht_type")=="BMP280")
      hasioSetupEntitie("press",          "Presure", "hPa","pressure");
    hasioSetupEntitie("soil",             "Soil moisture", "%",  "moisture");
    hasioSetupEntitie("salt",             "Soil salt", "µS/cm");
    hasioSetupEntitie("temp",             "Temperature", "°C", "temperature");

    // Diagnostics fields
    hasioSetupEntitie("IpAddress",        "Ip address", "x",  "x", "/state");
    hasioSetupEntitie("RSSI",             "Signal strength", "dBm","signal_strength", "/state");
    hasioSetupEntitie("batPerc",          "Battery", "%",  "battery", "/state");
    #ifdef VERBOSE_MODE
      hasioSetupEntitie("freeSpace",      "Free space", "Kb", "x", "/state");
      hasioSetupEntitie("batChargeDate",  "Battery charge date", "x", "date", "/state");
      hasioSetupEntitie("batADC",         "Battery adc", " ", "x", "/state");
      hasioSetupEntitie("onPower",        "Device on power", "x", "x", "/state");
      hasioSetupEntitie("batVolt",        "Battery volt", "V", "voltage", "/state");
      hasioSetupEntitie("loopMillis",     "Loop time", "ms", "x", "/state");
      hasioSetupEntitie("sleepReason",    "Sleep reason", "x", "x", "/state");
      hasioSetupEntitie("lastError",      "Last error", "x", "x", "/state");
    #endif
    hasioSetupEntitie("batDays",          "Days on battery", "d",  "x", "/state");
    hasioSetupEntitie("bootCount",        "Boots count", " ", "x", "/state");
    hasioSetupEntitie("bootCountError",   "Boots count error", " ", "x", "/state");

    hasioNetworkStatus();

    mqttClient.publish( getMqttPath(F("/lwt")).c_str(), "online", true);
}

// Get a json with sensors
// 0 - All, 1 - Sensors, 2 - Diagnostics
String getJsonBuff(const byte type ){

  //StaticJsonDocument<1792> doc;
  JsonDocument doc;
  //Set the values in the document according to SensorData
  JsonObject plant = doc.to<JsonObject>();
  if(type == 0 || type == 1){
    plant["sensorName"] = conf("plant_name");
    plant["time"] = data.time;
    plant["lux"] = truncateFloat(data.lux + atof(conf("offs_lux").c_str()), 1);
    plant["temp"] = truncateFloat(data.temp + atof(conf("offs_temp").c_str()), 1);
    plant["humid"] = truncateFloat(data.humid + atof(conf("offs_humid").c_str()), 1);
    if(conf("dht_type")=="BMP280")
      plant["press"] = truncateFloat(data.pressure + atof(conf("offs_pressure").c_str()), 1);
    plant["soil"] = (data.soil + conf("offs_soil").toInt());
    plant["salt"] = (data.salt + conf("offs_salt").toInt());
    // plant["soilTemp"] = config.soilTemp;
    // plant["saltadvice"] = config.saltadvice;
    // plant["plantValveNo"] = plantValveNo;
  }

  if(type == 0 || type == 2){
    plant["batPerc"] = data.batPerc;
    #ifdef VERBOSE_MODE
      plant["freeSpace"] = (STORAGE.totalBytes() - STORAGE.usedBytes()) / 1024;
      plant["batChargeDate"] = data.batChargeDate;
      plant["batADC"] = data.batAdcVolt;
      plant["batVolt"] = truncateFloat(data.batVolt, 2);
      plant["onPower"] = onPower;
      plant["loopMillis"] = millis() - appStart;
      plant["sleepReason"] = data.sleepReason;
      plant["lastError"] = data.lastError;
    #endif
    plant["batDays"] = truncateFloat(data.batDays, 1);
    plant["bootCount"] = data.bootCnt;
    plant["bootCountError"] = data.bootCntError;
    plant["RSSI"] = WiFi.RSSI(); //wifiRSSI;
    plant["IpAddress"] = WiFi.localIP().toString();

  }

  // Send to mqtt as string
  char buffer[2048];
  serializeJson(doc, buffer);
  return String(buffer);
}

// Publish sensors data to mqtt
void publishSensors() {
  //Publish sensors
  String topicS = getMqttPath(F("/sensors"));
  String jsonBuff = getJsonBuff(1);

  bool retained =  false;
  mqttPublish(topicS.c_str(), jsonBuff.c_str(), retained );

  //Publish state
  topicS = getMqttPath(F("/state"));
  jsonBuff = getJsonBuff(2);
  mqttPublish(topicS.c_str(), jsonBuff.c_str(), retained );

}
