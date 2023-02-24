#define CHUNKSIZE (1024 * 4)  //Size of chunks to send in view files
// Get a param units
String valueUnits(String key, bool u = true){
  if(key=="lux"){
    return u ? " lx":    "Luminosity";
  }else if(key=="temp"){
    return u ? " °C" :   "Temperature";
  }else if(key=="humid"){
    return u ? " %":     "Humidity";
  }else if(key=="batPerc"){    
    return u ? " %":     "Battery";
  }else if(key=="pressure"){
    return u ? " hPa":   "Pressure";
  }else if(key=="soil"){
    return u ? " %":     "Soil moisture";
  }else if(key=="salt"){
    return u ? " µS/cm": "Soil conductivity";
  }else if(key=="RSSI"){
    return u ? " dBm":   "Signal";
  }else if(key=="batDays"){
   return u ? " days":   "Battery Days";    
  }else if(key=="time"){
   return u ? "":        "Date/Time";    
  }else if(key=="sensorName"){
   return u ? "":        "Name";    
  }else if(key=="BatDays"){
   return u ? "":        "Battery days";    
  }else if(key=="batVolt"){
   return u ? " Volt":   "Battery voltage";    
  }else if(key=="batChargeDate"){
   return u ? "":        "Last charged";    
  }
  return u ? "" : key;
}

// Get a param svg
const char* valueSVG(String key){
  if(key=="lux"){
    return HTML_PAGE_SVG_SUN;
  }else if(key=="temp" || key=="pressure"){
    return HTML_PAGE_SVG_THERMOMETER;
  }else if(key=="humid"){
    return HTML_PAGE_SVG_RAIN;
  }else if(key=="soil"){
    return HTML_PAGE_SVG_PERC_DROP; 
  }else if(key=="salt"){
    return HTML_PAGE_SVG_EYE;
  }else if(key=="time"){
    return HTML_PAGE_SVG_CLOCK;
  }
  return NULL;
}
//Is device main sensor
bool isSensor(String key){
  if(key == "lux" || key== "temp" || key == "pressure" || key == "humid" ||
     key == "soil"|| key =="salt" || key == "time")
    return  true;
  return false;
}

// Connect to a SIID network
void connectToNetwork(){  
  WiFi.mode(WIFI_STA);
  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // call is only a workaround for bug in WiFi class
  WiFi.setHostname(conf["host_name"].c_str());
  String st_ssid =""; 
  String st_pass="";
  for (int i = 1; i < MAX_SSID_ARR_NO + 1; i++){    
    st_ssid = conf["st_ssid" + String(i)];
    if(st_ssid=="") continue;
    st_pass = conf["st_pass" + String(i)];

    //Wifi down, reconnect here
    LOG_INF("Wifi ST connecting to: %s \n",st_ssid.c_str());
    WiFi.begin(st_ssid.c_str(), st_pass.c_str());      
    int col = 0;
    uint32_t startAttemptTime = millis();      
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < CONNECT_TIMEOUT)  {
      Serial.printf(".");
      if (++col >= 60){ // just keep terminal from scrolling sideways        
        col = 0;
        Serial.printf("\n");
      }        
      Serial.flush();
      delay(500);    
    }
    Serial.printf("\n");
    if(WiFi.status() == WL_CONNECTED) break;
  }
  
  if (WiFi.status() != WL_CONNECTED){
    goToDeepSleep("notConnected");
  }else{
    LOG_INF("Wifi AP SSID: %s connected, use 'http://%s' to connect\n", st_ssid.c_str(), WiFi.localIP().toString().c_str()); 
  }
}

// Functions forward definition
String getJsonBuff();
void mqttSetupDevice(String chipId);

//Get one line html string
String getLineHtml(JsonPair kv){
  String line(HTML_PAGE_HOME_LINE);
  const char *key = kv.key().c_str();
  line.replace("{key}", key);
  String svg(valueSVG(key));
  line.replace("{svg}", svg.c_str());
  String name = valueUnits(key, false);
  line.replace("{name}", name.c_str());
  String units = valueUnits(key);
  line.replace("{units}", units.c_str());
  //Value type?
  if(kv.value().is<bool>())
    line.replace("{val}", String(kv.value().as<bool>()));
  else if(kv.value().is<int>())
    line.replace("{val}", String(kv.value().as<int>()));
  else if(kv.value().is<long>())
    line.replace("{val}", String(kv.value().as<long>()));
  else if(kv.value().is<float>())
    line.replace("{val}", String(kv.value().as<float>(),1));
  else if(kv.value().is<double>())
    line.replace("{val}", String(kv.value().as<double>(), 1));
  else if(kv.value().is<const char*>())
    line.replace("{val}", String(kv.value().as<const char*>()));
  return line;
}

// Handler function for root of the web page 
static void handleRoot(){
  //Read on each refresh
  //readSensors();
  String jsonBuff = getJsonBuff();
  DeserializationError error;
  DynamicJsonDocument doc(1024);
  //Parse json data
  error = deserializeJson(doc, jsonBuff.c_str());  
  if (error) { 
    LOG_ERR("Deserialize Json failed: %s\n", error.c_str());
    return;
  }
  JsonObject root = doc.as<JsonObject>();
  String out(HTML_PAGE_HOME_START);
  
  out.replace("{page_title}","Sensors of "+ conf["host_name"]);
  out.replace("{plant_name}",conf["plant_name"]);
  
  pServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  pServer->sendContent(out);
  pServer->sendContent(CONFIGASSIST_HTML_CSS);
  pServer->sendContent(HTML_PAGE_HOME_CSS);
  pServer->sendContent(HTML_PAGE_HOME_SCRIPT);
  out = HTML_PAGE_HOME_BODY;
  out.replace("{host_name}",conf["host_name"]);
  out.replace("{plant_name}",conf["plant_name"]);
  pServer->sendContent(out);

  for (JsonPair kv : root) {
    if(!isSensor(kv.key().c_str())) continue;
      String line = getLineHtml(kv);
      pServer->sendContent(line);
  }
  String sep(HTML_PAGE_HOME_SEP);
  pServer->sendContent(sep);
  for (JsonPair kv : root) {
    if(isSensor(kv.key().c_str())) continue;
      String line = getLineHtml(kv);
      pServer->sendContent(line);
  }

  String end(HTML_PAGE_HOME_END);
  end.replace("{appVer}", APP_VER );
  pServer->sendContent(end);
  //pServer->sendContent(HTML_PING_SCRIPT);
  pServer->client().flush(); 
  ResetCountdownTimer();
}

//Show a text file in browser window
static void handleViewFile(String fileName, bool download=false){
  File f = STORAGE.open(fileName.c_str());
  if (!f) {
    f.close();
    const char* resp_str = "File does not exist or cannot be opened";
    LOG_ERR("%s: %s", resp_str, fileName.c_str());
    pServer->send(200, "text/html", resp_str);
    pServer->client().flush(); 
    return;  
  }
  if (download) {  
    // download file as attachment, required file name in inFileName
    LOG_INF("Download file: %s, size: %0.1f K", fileName.c_str(), (float)(f.size()/(1024)));
    pServer->sendHeader("Content-Type", "text/text");
    pServer->sendHeader("Content-Disposition", "attachment; filename=" + fileName);
    pServer->sendHeader("Content-Length", String(f.size()));
    pServer->sendHeader("Connection", "close");
    size_t sz = pServer->streamFile(f, "application/octet-stream");
    if (sz != f.size()) {
      LOG_ERR("File: %s, Sent %lu, expected: %lu!\n", fileName.c_str(), sz, f.size()); 
    } 
    f.close();
    return;
  }

  LOG_INF("View file: %s, size: %0.1f K\n", fileName.c_str(), (float)(f.size()/(1024)));
  pServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  byte chunk[CHUNKSIZE];
  size_t chunksize;
  do {
    chunksize = f.read(chunk, CHUNKSIZE); 
    pServer->sendContent((char *)chunk, chunksize);
  } while (chunksize != 0);
  f.close();  
  pServer->client().flush(); 
}

// Handler function for commands from the main web page
static void handleCmd(){
  //WebServer::args() ignores empty parameters 
  for (int i = 0; i < pServer->args(); i++) {
    LOG_DBG("Cmds received: [%i] %s, %s \n",i, pServer->argName(i), pServer->arg(i).c_str()  );
    String out(CONFIGASSIST_HTML_MESSAGE);
    out.replace("{url}", "/");
    out.replace("{refresh}", "3000");
    String cmd(pServer->argName(i));    
    if(cmd=="view"){
      String file(pServer->arg(i));
      if(file=="") file = getLogFileName(false);
      handleViewFile(file);
      pServer->sendContent("\n\nFile: " + file );
      pServer->client().flush();
      return;
    }else if(cmd=="download"){
      String file(pServer->arg(i));
      if(file=="") file = getLogFileName(false);
      handleViewFile(file, true);
      return;
   }else if(cmd=="reboot"){
      out.replace("{msg}", "Rebooting ESP..<br>Please wait");
      out.replace("{title}", "Reboot");
      pServer->send(200, "text/html", out);
      pServer->client().flush(); 
      delay(1000);
      ESP.restart();
      return;
    }else if(cmd=="hasDiscovery"){
      mqttSetupDevice(getChipID()); 
      out.replace("{msg}", "Sended mqtt homeassistant device discovery");
      out.replace("{title}", "Homeassistant discovery");
    }else if(cmd=="del"){
      String file(pServer->arg(i));
      if(STORAGE.remove(file.c_str())){
        out.replace("{msg}", "Deleted file: " + file);
      }else{
        out.replace("{msg}", "Failed to delete file: " + file);
      }
      out.replace("{title}", "Delete");
   }else if(cmd=="reset"){
      out.replace("{msg}", "Deleting ini files..<br>Please wait");
      out.replace("{title}", "Reset ini files");
      reset();
   }else if(cmd=="resetLog"){
      out.replace("{title}", "Reseting log");
      if (logFile){
        dbgLog.close();
        STORAGE.remove(LOG_FILENAME);
        dbgLog = STORAGE.open(LOG_FILENAME, FILE_APPEND);
        if( !dbgLog ) {
          out.replace("{msg}", "Failed to reset log file..");
          Serial.printf("Failed to open log %s\n", LOG_FILENAME);
          logFile = false;
        }else{
          out.replace("{msg}", "Reseted log file");
        }
      }else{
        STORAGE.remove(LOG_FILENAME);
        out.replace("{msg}", "Deleted log file");
      }
    }else{
      out.replace("{title}", "Error");
      out.replace("{msg}", "Unknown command:" + cmd);
    }
    pServer->send(200, "text/html", out);
  } 
  pServer->send(200, "text/html", "");
  pServer->client().flush(); 
  ResetCountdownTimer();
}

// Handler function for ping from the web page to avoid sleep
static void handlePing(){
  pServer->send(200, "text/html", "");
  ResetCountdownTimer();
}

// Handler function for AP config form
static void handleAssistRoot() { 
  ResetCountdownTimer();
  conf.handleFormRequest(pServer); 
  pServer->sendContent(String(HTML_PING_SCRIPT));
}
// Handler function to list file sytem.
static void handleFileSytem(){
  String dir(DATA_DIR);
  if(pServer->hasArg("dir")){
    dir = pServer->arg("dir");
    if(dir.endsWith("/") && dir.length()>1) dir.remove(dir.length() - 1);
  }
  std::vector<String> dirArr;               //Directory array
  std::vector<std::vector<String>> fileArr; //Files array
  
  listSortedDir(dir, dirArr, fileArr );
  pServer->setContentLength(CONTENT_LENGTH_UNKNOWN);

  String out = HTML_PAGE_HOME_START;
  out.replace("{page_title}", "Directory: " + dir);
  pServer->sendContent(out);
  out ="";
  pServer->sendContent(CONFIGASSIST_HTML_CSS);
  pServer->sendContent(HTML_PAGE_SIMPLE_BODY);
  out += "<style> a { text-decoration: none; }</style>\n";
  out += "Directory: <b>" + dir + "</b></br></br>\n";
  pServer->sendContent(out);
  out = "<table>";
  uint8_t row = 0;
  while (row++ < dirArr.size()) { 
    String d = dirArr[row - 1];    
    if(row==1){
      String up = d.substring( 0, d.lastIndexOf("/") );
      if(up != "" && up != dir){
        out += "<tr><td colspan='5'><a href='/fs?dir=" + up + "'><b>[ .. ]</b></a></td>";
        out += "</tr>\n";
      } 
    }
    if(d!=dir){
      out += "<tr><td><a href='/fs?dir=" + d + "'>" + String(HTML_PAGE_SVG_FOLDER) + "</a></td>";
      out += "<td style='text-align: left;'><a href='/fs?dir=" + d + "'><b> " + d + "</b></a></td></tr>\n";
      
    } 
    pServer->sendContent(out);
    out = "";
  }
  out = "</table>";
  pServer->sendContent(out);
  row=0;
  while (row++ < fileArr.size()) { 
    String f = fileArr[row - 1][0];
    String s = fileArr[row - 1][1];
    out="";
    if(row==1) out+="<table>";
    out += "<tr>";
    out += "<td><a target='_blank;' title='View' href='/cmd?view=" + dir + "/" + f + "'><b>" + f + "</b></a></td><td>&nbsp;&nbsp" + s + " Kb</td>\n";
    out += "<td><a title='Download' href='/cmd?download=" + dir + "/" + f + "'>" + HTML_PAGE_SVG_DOWNLOAD + "</a></td>\n";
    #if defined(DEBUG_FILES)
      out += "<td><a title='Delete' href='/cmd?del=" + dir + "/" + f + "'>"+ HTML_PAGE_SVG_RECYCLE +"</a></td>\n";
    #endif
    out += "</tr>";
    pServer->sendContent(out);
  }
  out = "</table></br>";
  if(dir == DATA_DIR)
    out += "</br>" + String(HTML_HOME_BUTTON);  
  else 
    out += "</br>" + String(HTML_BACK_BUTTON);  
  pServer->sendContent(out);
  out = "</br></br>";
  size_t free = STORAGE.totalBytes() - STORAGE.usedBytes();
  out += "<small>Total space: " + String(STORAGE.totalBytes() / 1024) + " K, ";  
  out += "Used: " + String(STORAGE.usedBytes() / 1024) + " K, ";  
  out += "Free: " + String(free / 1024) + " K</small></br>";  
  out += "" + String(HTML_PING_SCRIPT);
  pServer->sendContent(out);
  pServer->sendContent(HTML_PAGE_SIMPLE_BODY_END);
  pServer->client().flush(); 
}

// Websockets handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            LOG_INF("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
              IPAddress ip = pWebSocket->remoteIP(num);
              LOG_INF("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

              // send message to client
              pWebSocket->sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            LOG_INF("[%u] get Text: %s\n", num, payload);
            ResetCountdownTimer();
            // send message to client
            // webSocket.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
            LOG_INF("[%u] get binary length: %u\n", num, length);
            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
    }
}

//Send sensor readings to socket
void wsSendSensors(){
  if(!pWebSocket) return;
  String jsonBuff = getJsonBuff();
  pWebSocket->broadcastTXT(jsonBuff);
}

// Is a web server client connected (AP or SP)?
bool isClientConnected(WEB_SERVER *pServer){

  apClients = WiFi.softAPgetStationNum();
  if(apClients>0){
    LOG_DBG("Connected, AP Clients: %u\n", apClients);
    return true;
  }
  if(pServer==NULL) return false;

  WiFiClient myclient = pServer->client();
  if(myclient && myclient.connected()){
    LOG_INF("Connected, ST client\n");
    return true;
  }
  return false;
}
// Start the web server
void startWebSever(){
  if(pServer) return;
  if (MDNS.begin(conf["host_name"].c_str()))  
    LOG_INF("MDNS responder Started\n");
  pServer = new WebServer(80);
  pServer->begin();
  pServer->on("/", handleRoot);
  pServer->on("/cmd", handleCmd);
  pServer->on("/cfg", handleAssistRoot);
  pServer->on("/pg", handlePing);
  pServer->on("/fs", handleFileSytem);
  LOG_INF("Started web server at port: 80\n");
  pWebSocket = new WebSocketsServer(81);
  pWebSocket->begin();
  pWebSocket->onEvent(webSocketEvent);
  LOG_INF("Started web sockets at port: 81\n");  
}