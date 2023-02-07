#define CHUNKSIZE (1024 * 4)  //Size of chunks to send in view files

// Template for header, begin of the config form
PROGMEM const char HTML_PAGE_DEF_START[] = R"=====(
<!DOCTYPE HTML>
<html lang='de'>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sensors of {host_name}</title>
<style>
:root {
 --bg-table-stripe: #f6f6f5;
 --b-table: #999a9b47;
 --caption: #242423;
}
body {
  font-family: "Open Sans", sans-serif;
  line-height: 1.25;
}
table {
  border-bottom: 1px solid var(--b-table);
  border-top: 1px solid var(--b-table);  
  border-collapse: collapse;
  margin: 0;
  padding: 0;
  table-layout: fixed;
}
table caption {  
  margin: .4em 0 .35em;
  color: var(--caption);
}
table tfoot{
  padding: 5px;  
}
table th, table td {
  padding: .425em;
  border: 1px solid var(--b-table);
}
tbody tr:nth-of-type(2n+1) {
  background-color: var(--bg-table-stripe)
}
.pair-sep{
    text-align: center;
    font-weight: 800;
    font-size: 16px;
    border-bottom: 2px solid lightgray;
    background-color: beige;
}
.pair-key{
    text-align: right;
}
.pair-val{
    text-align: left;
    font-weight: 800;
}
.pair-lbl{
    text-align: left;
    font-style: italic;}


 @media screen and (max-width: 400px) {
  table {
    border: 0;
  }

  table caption {
    font-size: 1.3em;
  }

  table thead {
    border: none;
    clip: rect(0 0 0 0);
    height: 1px;
    margin: -1px;
    overflow: hidden;
    padding: 0;
    position: absolute;
    width: 1px;
  }

  table tr {
    border-bottom: 3px solid #ddd;
    display: block;
    margin-bottom: 0.125em;
  }

  table td {
    border-bottom: 1px solid #ddd;
    display: block;
    font-size: 0.8em;
    text-align: right;
  }

  table td::before {
    content: attr(data-label);
    float: left;
    font-weight: bold;
    text-transform: uppercase;
  }

  table td:last-child {
    border-bottom: 0;
  }
  .pair-key{
    text-align: left;    
  }
}
.w-100 {
  width: 100%!important;
}
button {
    background-color: #fff;
    border: 1px solid #d5d9d9;
    border-radius: 4px;
    box-shadow: rgb(213 217 217 / 50%) 0 2px 5px 0;
    box-sizing: border-box;
    font-weight: 900;
    color: #0f1111;
    cursor: pointer;
    display: inline-block;
    padding: 0 10px 0 11px;
    margin: 2px;
    line-height: 29px;
    position: relative;
    text-align: center;
    vertical-align: middle;
        
}
button:hover {
    background-color: #f7fafa;
}
</style>
</head>
<script>
/*********** WebSockets functions ***********/
const wsServer = "ws://" + document.location.host + ":81/";
let ws = null;
let hbTimer = null;
let refreshInterval = 5000;
document.addEventListener('DOMContentLoaded', initWebSocket());

// define websocket handling
function initWebSocket() {  
  console.log("Connect to: " + wsServer);
  ws = new WebSocket(wsServer);
  ws.onopen = onOpen;
  ws.onclose = onClose;
  ws.onmessage = onMessage; 
  ws.onerror = onError;
}
function sendCmd(reqStr) {
  ws.send(reqStr);
  console.log("Cmd: " + reqStr);
}

// periodically check that connection is still up and get status
function heartbeat() {
  if (!ws) return;
  if (ws.readyState !== 1) return;
  sendCmd("H");
  clearTimeout(hbTimer);
  hbTimer = setTimeout(heartbeat, refreshInterval);
}

// connect to websocket server
function onOpen(event) {
  console.log("Connected");
  //heartbeat();
}

// process received WS message
function onMessage(messageEvent) {
  if (messageEvent.data.startsWith("{")) {
    // json data
    updateData = JSON.parse(messageEvent.data);
    updateTable(updateData); // format received config json into html table
  } else {
    console.log(messageEvent.data);
  }

  if(hbTimer){
    clearTimeout(hbTimer);
    hbTimer = setTimeout(heartbeat, refreshInterval);
  }
}

function valueUnits(key){
  if(key=="lux"){
    return " lx";
  }else if(key=="temp"){
    return " °C";
  }else if(key=="humid" || key=="batPerc"){
    return " %";
  }else if(key=="pressure"){
    return " hPa";
  }else if(key=="RSSI"){
    return " dBm";    
  }else if(key=="batVolt"){
    return " Volt";    
  }else if(key=="batDays"){
    return " days";    
  }
  return "";
}
function updateTable(configData){
  Object.entries(configData).forEach(([key, value]) => {
    const td = document.getElementById(key)
    if(td) td.innerHTML = value + valueUnits(key);
  });
}

function onError(event) {
  console.log("WS Error: " + event.code);
}

function onClose(event) {
  console.log("Disconnected: " + event.code + ' - ' + event.reason);
  ws = null;
  // event.codes:
  //   1006 if server not available, or another web page is already open
  //   1005 if closed from app
  if (event.code == 1006) {} //alert("Closed websocket as a newer connection was made, refresh browser page");
  else if (event.code != 1005) initWebSocket(); // retry if any other reason
}
</script>        
<body>
<center>
    <div class="w-100">
      <table>
        <caption>
          <h3>{host_name} status</h3>
        </caption>
        <tbody>
)=====";
// Template for one line
PROGMEM const char HTML_PAGE_DEF_LINE[] = R"=====(
        <tr>
          <td scope="row" class="pair-key">{key}</td>
          <td class="pair-val" id="{key}">{val}</td>    
        </tr>    
)=====";

// Template for page end
PROGMEM const char HTML_PAGE_DEF_END[] = R"=====(
      </tbody>
          <tfoot>
            <tr>
              <td style="text-align: center;" colspan="5">
              <button title='View sensors daily log' onClick='window.location.href="/cmd?view="'>Day</button>
              <button title='View sensors monthly logs' onClick='window.location.href="/fs?dir=/data"'>Logs</button>
              <button title='Send device mqtt discovery message to Homeassistant' onClick='window.location.href="/cmd?hasDiscovery=1"'>Discover</button>
              <button title='Configure this device' onClick='window.location.href="/cfg"'>Configure</button>
              <button title='Reboot device' onClick='window.location.href="/cmd?reboot=1"'>Reboot</button>
              <button title='Factory defaults' onClick='window.location.href="/cmd?reset=1"'>Reset</button>
            </tr>
          </tfoot>
        </table>
        </br>
        <span><small>PlantStatus Ver: {appVer}</small></span>
      </div>
</center>
</body>
</html>
)=====";


//Back button
PROGMEM const char HTML_BACK_SCRIPT[] = R"~(
<small><a href="" onClick="window.history.go(-1); return false;">[&nbsp;Back&nbsp;]</a></small>
)~";

// Ping script
PROGMEM const char HTML_PING_SCRIPT[] = R"~(
<script>
function ping(){
  const Http = new XMLHttpRequest();
  const url='/pg';
  Http.open("GET", url);
  Http.send();
}
setInterval(ping, 10000);
</script>
)~";

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

String valueUnits(String key){
  if(key=="lux"){
    return  " lx";
  }else if(key=="temp"){
    return " °C";
  }else if(key=="humid" || key=="batPerc"){
    return " %";
  }else if(key=="pressure"){
    return " hPa";
  }else if(key=="RSSI"){
    return " dBm";    
  }else if(key=="batVolt"){
    return " Volt";    
  }else if(key=="batDays"){
    return " days";    
  }
  return "";
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
  String out(HTML_PAGE_DEF_START);
  out.replace("{host_name}",conf["host_name"]);
  pServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  pServer->sendContent(out);
  for (JsonPair kv : root) {
      String line(HTML_PAGE_DEF_LINE);
      line.replace("{key}", kv.key().c_str());
      String units = valueUnits(kv.key().c_str());
      //Value type?
      if(kv.value().is<bool>())
        line.replace("{val}", String(kv.value().as<bool>()) + units);
      else if(kv.value().is<int>())
        line.replace("{val}", String(kv.value().as<int>()) + units);
      else if(kv.value().is<long>())
        line.replace("{val}", String(kv.value().as<long>()) + units);
      else if(kv.value().is<float>())
        line.replace("{val}", String(kv.value().as<float>(),1) + units);
      else if(kv.value().is<double>())
        line.replace("{val}", String(kv.value().as<double>(), 1) + units);
      else if(kv.value().is<const char*>())
        line.replace("{val}", String(kv.value().as<const char*>()) + units);
      pServer->sendContent(line);
  }
  String end(HTML_PAGE_DEF_END);
  end.replace("{appVer}", APP_VER );
  pServer->sendContent(end);
  pServer->sendContent(HTML_PING_SCRIPT);
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
    String out(HTML_PAGE_MESSAGE);
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
      out.replace("{title}", "Reboot");
      reset();
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
    if(dir.endsWith("/")) dir.remove(dir.length() - 1);
  }
  std::vector<String> dirArr;               //Directory array
  std::vector<std::vector<String>> fileArr; //Files array
  
  listSortedDir(dir, dirArr, fileArr );
  pServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  pServer->sendHeader("Content-Type", "text/html");
  String out ="";
  out += "<style> a { text-decoration: none; }</style>\n";
  out += "Directory: <b>" + dir + "</b></br></br>\n";
  uint8_t row = 0;
  while (row++ < dirArr.size()) { 
    String d = dirArr[row - 1];    
    if(row==1){
      String up = d.substring( 0, d.lastIndexOf("/") );
      if(up != "" && up != dir)  out += "<a href='/fs?dir=" + up + "'><b>[ .. ]</b></a></br>\n";
    }
    if(d!=dir) out += "<a href='/fs?dir=" + d + "'><b>[ " + d + " ]</b></a></br>\n";
    pServer->sendContent(out.c_str(), out.length());
    out = "";
  }
  row=0;
  while (row++ < fileArr.size()) { 
    String f = fileArr[row - 1][0];
    String s = fileArr[row - 1][1];
    out="";
    if(row==1) out+="</br>";
    out += "<a title='View' href='/cmd?view=" + dir + "/" + f + "'><b>" + f + "&nbsp;&nbsp" + s + " Kb</b></a>&nbsp;&nbsp;&nbsp;";
    out += "<a title='Download' href='/cmd?download=" + dir + "/" + f + "'>[ Down ]</a>&nbsp;";
    #if defined(DEBUG_FILES)
      out += "<a title='Delete' href='/cmd?del=" + dir + "/" + f + "'>[ Del ]</a>\n";
    #endif
    out += "</br>";
    pServer->sendContent(out.c_str(), out.length());
  }
  size_t free = STORAGE.totalBytes() - STORAGE.usedBytes();
  out = "</br>";
  out += "<small>Total space: " + String(STORAGE.totalBytes() / 1024) + " K, ";  
  out += "Used: " + String(STORAGE.usedBytes() / 1024) + " K, ";  
  out += "Free: " + String(free / 1024) + " K</small></br>";  
  out += "</br>" + String(HTML_BACK_SCRIPT);
  out += "</br>" + String(HTML_PING_SCRIPT);
  pServer->sendContent(out.c_str(), out.length());  
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