// Needs to be a time zone string from: https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
time_t getEpoch() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec;
}

// Display local time
static void showLocalTime(const char* timeSrc) {
  time_t currEpoch = getEpoch();
  char timeFormat[20];
  strftime(timeFormat, sizeof(timeFormat), "%d/%m/%Y %H:%M:%S", localtime(&currEpoch));
  LOG_INF("Got current time from %s: %s \n", timeSrc, timeFormat);
  timeSynchronized = true;
}

// Get a string of current chip id
String getChipID(){
  byte mac[6];
  WiFi.macAddress(mac);

  String chipId = "";
  String HEXcheck = "";
  for (int i = 0; i <= 5; i++) {
    HEXcheck = String(mac[i], HEX);
    if (HEXcheck.length() == 1) {
      chipId = chipId + "0" + String(mac[i], HEX);
    } else {
      chipId = chipId + String(mac[i], HEX);
    }
  }  
  return chipId;
}

// Get current time from NTP server and apply to ESP32
bool getLocalNTPTime() {
  if(timeZone=="") timeZone = conf["time_zone"];
  if(ntpServer=="") ntpServer = conf["ntp_server"];
  LOG_INF("Using NTP server: %s with tz: %s\n", ntpServer.c_str(), timeZone.c_str());
  configTzTime(timeZone.c_str(), ntpServer.c_str());
  if (getEpoch() > 10000) {
    showLocalTime("NTP");    
    return true;
  }
  else {
    LOG_WRN("Not yet synced with NTP\n");
    return false;
  }
}
// Sync time to ntp
void syncTime(){
  int tries=8;
  while(!getLocalNTPTime() && tries >= 0){
    delay(2000);
    tries--;
  };
  LOG_INF("Time sync: %i\n", timeSynchronized);
}
// Convert a string to time_t
time_t convertDateTimeString(String sDateTime){  
  tmElements_t tm;
  int Year, Month, Day, Hour, Minute, Second;
  sscanf(sDateTime.c_str(), "%d-%d-%d %d:%d:%d", &Year, &Month, &Day, &Hour, &Minute, &Second);
  tm.Year = CalendarYrToTm(Year);
  tm.Month = Month;
  tm.Day = Day;
  tm.Hour = Hour;
  tm.Minute = Minute;
  tm.Second = Second;
  return makeTime(tm);
}
// Get a string of current date/time
String getCurDateTimeString(bool isFolder = false) {
  // construct timestamp from date/time
  if(!timeSynchronized) return "";
  size_t buff_Len = 64;
  char buff[buff_Len];
  time_t currEpoch = getEpoch();    
  if(isFolder) strftime(buff, buff_Len, "/%Y_%m_%d/%Y_%m_%d-%H_%M_%S", localtime(&currEpoch));
  else strftime(buff, buff_Len, "%Y-%m-%d %H:%M:%S", localtime(&currEpoch));
  return String(buff);
}
// Get an advice on salt
String getSaltAdvice(uint32_t salt){
  String advice;
  if (salt < 201)        advice = "needed";
  else if (salt < 251)   advice = "low";  
  else if (salt < 351)   advice = "optimal";
  else if (salt > 350)   advice = "too high";  
  return advice;
}
// Truncate a float value
float truncate(float val, byte dec){
    float x = val * pow(10, dec);
    float y = round(x);
    float z = x - y;
    if ((int)z == 5)
    {
        y++;
    } else {}
    x = y / pow(10, dec);
    return x;
}

// Reset timer used for sleep
void ResetCountdownTimer(){
  sleepTimerCountdown = SLEEP_DELAY_INTERVAL; 
}

void reset(){
  LOG_DBG("Removing ini files.");
  listDir(SPIFFS, "/",1);
  lastBoot.deleteConfig(LAST_BOOT_CONF);
  conf.deleteConfig(CONF_FILE);
}

