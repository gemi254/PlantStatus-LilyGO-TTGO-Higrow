// Template for header, begin of the config form
PROGMEM const char HTML_PAGE_DEF_START[] = R"=====(
<!DOCTYPE HTML>
<html lang='de'>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sensors of {appName}</title>
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
<body>
<center>
    <div class="w-100">
      <table>
        <caption>
          <h3>{appName} sensors</h3>
        </caption>
        <tbody>
)=====";
// Template for one line
PROGMEM const char HTML_PAGE_DEF_LINE[] = R"=====(
  <tr>
    <td scope="row" class="pair-key">{key}</td>
    <td class="pair-val">{val}</td>    
  </tr>    
)=====";

// Template for page end
PROGMEM const char HTML_PAGE_DEF_END[] = R"=====(
      </tbody>
          <tfoot>
            <tr>
              <td style="text-align: center;" colspan="5">
              <button title='Configure this device' onClick='window.location.href="/cfg"'>Configure</button>
              <button title='Send device mqtt discovery message to Homeassistant' onClick='window.location.href="/cmd?hasDiscovery=1"'>HAS discovery</button>
              <button title='Reboot device' onClick='window.location.href="/cmd?reboot=1"'>Reboot</button>
              <button title='Factory defaults' onClick='window.location.href="/cmd?reset=1"'>Reset</button>
            </tr>
          </tfoot>
        </table>
        </br>
        <span><small>Ver: {appVer}</small></span>
      </div>
</center>
</body>
</html>
)=====";

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

// Mqtt function forward definition
String getJsonBuff();
void mqttSetupDevice(String chipId);

// Handler function for root of the web page 
static void handleRoot(){

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
  out.replace("{appName}",conf["host_name"]);
  pServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  pServer->sendContent(out);
  for (JsonPair kv : root) {
      String line(HTML_PAGE_DEF_LINE);
      line.replace("{key}", kv.key().c_str());
      //Value type?
      if(kv.value().is<bool>())
        line.replace("{val}", String(kv.value().as<bool>()));
      else if(kv.value().is<int>())
        line.replace("{val}", String(kv.value().as<int>()));
      else if(kv.value().is<long>())
        line.replace("{val}", String(kv.value().as<long>()));
      else if(kv.value().is<float>())
        line.replace("{val}", String(kv.value().as<float>()));
      else if(kv.value().is<double>())
        line.replace("{val}", String(kv.value().as<double>()));
      else if(kv.value().is<const char*>())
        line.replace("{val}", kv.value().as<const char*>());

      pServer->sendContent(line);
  }
  String end(HTML_PAGE_DEF_END);
  end.replace("{appVer}", APP_VER );
  pServer->sendContent(end);
  pServer->sendContent(HTML_PING_SCRIPT);
  pServer->client().flush(); 
  ResetCountdownTimer();
}

// Handler function for commands from the main web page
static void handleCmd(){
  for (int i = 0; i < pServer->args(); i++) {
    LOG_DBG("Cmds received: [%i] %s, %s ",i, pServer->argName(i), pServer->arg(i)  );
    String out(HTML_PAGE_MESSAGE);
    out.replace("{url}", "/");
    out.replace("{refresh}", "3000");
    if(pServer->argName(i)=="hasDiscovery"){
      mqttSetupDevice(getChipID()); 
      out.replace("{msg}", "Sended mqtt homeassistant device discovery");
      out.replace("{title}", "Homeassistant discovery");
      pServer->send(200, "text/html", out);
      pServer->client().flush(); 
      return;
    }else if(pServer->argName(i)=="reboot"){
      out.replace("{msg}", "Rebooting ESP..<br>Please wait");
      out.replace("{title}", "Reboot");
      pServer->send(200, "text/html", out);
      pServer->client().flush(); 
      delay(1000);
      ESP.restart();
      return;      
    }else if(pServer->argName(i)=="reset"){
      out.replace("{msg}", "Deleting ini files..<br>Please wait");
      out.replace("{title}", "Reboot");
      pServer->send(200, "text/html", out);
      pServer->client().flush(); 
      reset();
      return;      
    }

  } 
  pServer->send(200, "text/html", "");
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

// Start the web server
void startWebSever(){
  if(pServer) return;
  if (MDNS.begin(conf["host_name"].c_str()))  LOG_INF("MDNS responder Started\n");
  pServer = new WebServer(80);
  pServer->begin();
  pServer->on("/", handleRoot);
  pServer->on("/cmd", handleCmd);
  pServer->on("/cfg", handleAssistRoot);
  pServer->on("/pg", handlePing);
  LOG_INF("Started web server at port: 80\n");
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