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
  }else if(key=="freeSpace"){
   return u ? "Kb":        "Free space";
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

// Functions forward definitions
String getJsonBuff(const byte type = 0 );
void mqttSetupDevice();

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
// Handler function for AP config form
static void handleAssistRoot() {
  ResetCountdownTimer("configEdit");
  conf.handleFormRequest(pServer);
  if(pServer->args() == 0)
    pServer->sendContent(String(HTML_PING_SCRIPT));
}

// Handler function for root of the web page
static void handleRoot(){
  String jsonBuff = getJsonBuff();
  DeserializationError error;
  JsonDocument doc;
  //Parse json data
  error = deserializeJson(doc, jsonBuff.c_str());
  if (error) {
    LOG_E("Deserialize Json failed: %s\n", error.c_str());
    return;
  }
  JsonObject root = doc.as<JsonObject>();
  String out(HTML_PAGE_HOME_START);

  out.replace("{page_title}","Sensors of "+ conf("host_name"));
  out.replace("{plant_name}",conf("plant_name"));

  pServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  pServer->sendContent(out);
  pServer->sendContent(conf.getCSS());
  pServer->sendContent(HTML_PAGE_HOME_CSS);
  pServer->sendContent(HTML_PAGE_HOME_SCRIPT);
  //Send time sync script on AP
#ifdef CA_USE_TIMESYNC
  if(conf.isAPEnabled()) pServer->sendContent("<script>" + conf.getTimeSyncScript() + "</script>");
#endif
  out = HTML_PAGE_HOME_BODY;
  out.replace("{host_name}",conf("host_name"));
  out.replace("{plant_name}",conf("plant_name"));
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
  if(STORAGE.exists(LOGGER_LOG_FILENAME)){
    String btView(HTML_PAGE_HOME_BUTTON_VIEWLOG);
    String btReset(HTML_PAGE_HOME_BUTTON_RESETLOG);
    btView.replace("{log}", LOGGER_LOG_FILENAME);
    btReset.replace("{log}", LOGGER_LOG_FILENAME);
    end.replace("<!--Custom-->", "<br>\n" + btView + "\n" + btReset);
  }
  pServer->sendContent(end);
  //pServer->sendContent(HTML_PING_SCRIPT);
  pServer->client().flush();
  ResetCountdownTimer("Handle root");
}

//Show a text file in browser window
static void handleViewFile(String fileName, bool download=false){
  File f = STORAGE.open(fileName.c_str());
  if (!f) {
    f.close();
    const char* resp_str = "File does not exist or cannot be opened";
    LOG_E("%s: %s", resp_str, fileName.c_str());
    pServer->send(200, "text/html", resp_str);
    pServer->client().flush();
    return;
  }
  if (download) {
    // download file as attachment, required file name in inFileName
    LOG_I("Download file: %s, size: %0.1f K", fileName.c_str(), (float)(f.size()/(1024)));
    pServer->sendHeader("Content-Type", "text/text");
    int n = fileName.lastIndexOf( '/' );
    String downloadName = conf("host_name") + "_" + fileName.substring( n + 1 );
    pServer->sendHeader("Content-Disposition", "attachment; filename=" + downloadName);
    pServer->sendHeader("Content-Length", String(f.size()));
    pServer->sendHeader("Connection", "close");
    size_t sz = pServer->streamFile(f, "application/octet-stream");
    if (sz != f.size()) {
      LOG_E("File: %s, Sent %lu, expected: %lu!\n", fileName.c_str(), sz, f.size());
    }
    f.close();
    return;
  }

  LOG_I("View file: %s, size: %0.1f K\n", fileName.c_str(), (float)(f.size()/(1024)));
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

  for (int i = 0; i < pServer->args(); i++) { // WebServer::args() ignores empty parameters
    LOG_D("Cmds received: [%i] %s, %s \n",i, pServer->argName(i), pServer->arg(i).c_str()  );
    String out(conf.getMessageHtml());
    String cmd(pServer->argName(i));

    out.replace("{refresh}", "3000");
    out.replace("{reboot}","false");

    if(cmd=="del" || cmd=="rm"){
      String file(pServer->arg(i));
      if(STORAGE.remove(file.c_str())){
        out.replace("{msg}", "Deleted file: " + file);
      }else{
        out.replace("{msg}", "Failed to delete file: " + file);
      }
      out.replace("{title}", "Delete");
      String filePath = pServer->arg("del");
      String dirPath = filePath.substring( 0, filePath.lastIndexOf("/") );
      out.replace("{url}", "/fs?dir=" + dirPath);
      pServer->send(200, "text/html", out);
      return;
    }

    out.replace("{url}", "/");
    if(cmd=="ls"){
      String dir(DATA_DIR);
      if(pServer->hasArg("dir")){
        dir = pServer->arg("dir");
        if(dir.endsWith("/") && dir.length()>1) dir.remove(dir.length() - 1);
      }
      std::vector<String> dirArr;               //Directory array
      std::vector<std::vector<String>> fileArr; //Files array

      listSortedDir(dir, dirArr, fileArr );
      size_t row = 0;
      while (row++ < dirArr.size()) {
        String d = dirArr[row - 1];
        pServer->sendContent("[" + d + "]\n");
      }
      row = 0;
      while (row++ < fileArr.size()) {
        String f = fileArr[row - 1][0];
        String s = fileArr[row - 1][1];
        pServer->sendContent(f + "\t" + s + "\n");
      }
      return;
    }else if(cmd=="view"){
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
      mqttSetupDevice();
      out.replace("{msg}", "Sended mqtt homeassistant device discovery");
      out.replace("{title}", "Homeassistant discovery");
    }else if(cmd=="reset"){
      out.replace("{msg}", "Deleting ini files..<br>Please wait");
      out.replace("{title}", "Reset ini files");
      reset();
   }else if(cmd=="resetLog"){
      out.replace("{title}", "Reseting log");
      if(logFile) logFile.close();
      STORAGE.remove(LOGGER_LOG_FILENAME);
      #ifdef CA_USE_LITTLEFS
      if(!logFile) logFile = STORAGE.open(LOGGER_LOG_FILENAME, "a+", true);
      #else
      if(!logFile) logFile = STORAGE.open(LOGGER_LOG_FILENAME, "a+");
      #endif
      out.replace("{msg}", "Reset log file");
    }else{
      out.replace("{title}", "Error");
      out.replace("{msg}", "Unknown command:" + cmd);
    }
    pServer->send(200, "text/html", out);
  }
  //pServer->send(200, "text/html", "");
  //pServer->client().flush();
  ResetCountdownTimer("Handle cmd");
}

// Handler function for ping from the web page to avoid sleep
static void handlePing(){
  pServer->send(200, "text/html", "");
  ResetCountdownTimer("Handle ping");
}

// Handler function for ping from the web page to avoid sleep
static void handleFavIcon(){
  pServer->send(200, "text/html", "");
}

// Handler function to list file sytem.
static void handleFileSytem(){
  String dir(DATA_DIR);
  if(pServer->hasArg("dir")){
    dir = pServer->arg("dir");
    // Remove trailing slash
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
  pServer->sendContent(conf.getCSS());
  pServer->sendContent(HTML_PAGE_SIMPLE_BODY);
  out += "<style> a { text-decoration: none; }</style>\n";
  out += "Directory: <b>" + dir + "</b></br></br>\n";
  pServer->sendContent(out);
  out = "<table>";
  size_t row = 0;
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
    out += "<td><a target='_blank;' title='View' href='/cmd?view=" + f + "'><b>" + f + "</b></a></td><td>&nbsp;&nbsp" + s + " Kb</td>\n";
    out += "<td><a title='Download' href='/cmd?download=" + f + "'>" + HTML_PAGE_SVG_DOWNLOAD + "</a></td>\n";
    out += "<td><a title='Delete' href='/cmd?del=" + f + "'>"+ HTML_PAGE_SVG_RECYCLE +"</a></td>\n";
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

static void handleCharts(){
  String out = HTML_PAGE_HOME_START;
  out.replace("{page_title}", "Charts");
  pServer->sendContent(out);
  pServer->sendContent(HTML_CHARTS_CSS);
  pServer->sendContent("<script>" + String(HTML_CHARTS_SCRIPT) + "</script>");
  pServer->sendContent("<script>" + String(HTML_CHARTS_MAIN_SCRIPT) + "</script>");
  pServer->sendContent(HTML_CHARTS_BODY);
  pServer->sendContent(HTML_PING_SCRIPT);
}

// Websockets handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            LOG_D("Websocket [%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
              IPAddress ip = pWebSocket->remoteIP(num);
              LOG_D("Websocket [%u] connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

              // send message to client
              pWebSocket->sendTXT(num, "Connected");
            }
            break;
        case WStype_TEXT:
            LOG_D("[%u] get Text: %s\n", num, payload);
            ResetCountdownTimer("Websocket text");
            // send message to client
            // webSocket.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
            LOG_D("[%u] get binary length: %u\n", num, length);
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

// Is a web server client connected (AP or ST)?
bool isClientConnected(WEB_SERVER *pServer){

  apClients = WiFi.softAPgetStationNum();
  if(apClients>0){
    LOG_V("Connected, AP Clients: %u\n", apClients);
    return true;
  }
  if(pServer==NULL) return false;

  WiFiClient myclient = pServer->client();
  if(myclient && myclient.connected()){
    LOG_V("Connected, ST client\n");
    return true;
  }
  return false;
}

// Register web server handlers
void registerHandlers(){
  if(!pServer) return;
  pServer->on("/", handleRoot);
  pServer->on("/cmd", handleCmd);

  //Handlers are registered by configAssist
  if(!conf.isAPEnabled()){
    pServer->on("/cfg", handleAssistRoot );
    //pServer->on("/scan", handleAssistScan );
    conf.setup(*pServer);
  }
  pServer->on("/pg", handlePing);
  pServer->on("/fs", handleFileSytem);
  pServer->on("/chrt", handleCharts);
  pServer->on("/favicon.ico", handleFavIcon);
  LOG_D("Registered web handlers\n");
}

// Start the websocket server
void startWebSockets(){
  if(pWebSocket) return;
  pWebSocket = new WebSocketsServer(81);
  pWebSocket->begin();
  pWebSocket->onEvent(webSocketEvent);
  LOG_I("Started web sockets at port: 81\n");
}

// Start the web server
void startWebSever(){
  if(pServer) return;
  if (MDNS.begin(conf("host_name").c_str()))
    LOG_D("MDNS responder Started\n");
  pServer = new WebServer(80);
  pServer->begin();
  LOG_I("Started web server at port: 80\n");
  //Register server handlers
  registerHandlers();
  //Start websockets
  startWebSockets();
}
// Start Access point server and edit config
void startAP(){
    pServer = new WebServer(80);
    //Start AP and register configAssist handlers
    conf.setup(*pServer, true);

    //Register app webserver handlers
    registerHandlers();

    //Start websockets
    startWebSockets();

    ResetCountdownTimer("AP start");
    initSensors();
}