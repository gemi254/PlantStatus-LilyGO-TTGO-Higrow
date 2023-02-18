
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
// Get mac id
String getChipID(){
  String mac = WiFi.macAddress();
  mac.replace(":","");
  mac.toLowerCase();
  return mac; //.substring(12);
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
  if(tries==0) LOG_WRN("Time sync: %i\n", timeSynchronized);
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
String getCurDateTimeString(bool isFolder = false, bool isFile=false) {
  // construct timestamp from date/time
  if(!timeSynchronized) return "";
  size_t buff_Len = 64;
  char buff[buff_Len];
  time_t currEpoch = getEpoch();    
  if(isFolder) strftime(buff, buff_Len, "/%Y-%m/", localtime(&currEpoch));
  else if (isFile) strftime(buff, buff_Len, "%Y-%m-%d", localtime(&currEpoch));  
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

float truncateFloat(float v, int prec=1){  return atof(String(v, prec).c_str());   }

bool isNumeric(String s){ //1.0, -.232, .233, -32.32
  unsigned int l = s.length();
  if(l==0) return false;
  bool dec=false, sign=false;
  for(unsigned int i = 0; i < l; ++i) {
    if (s.charAt(i) == '.'){
      if(dec) return false;
      else dec = true;
    }else if(s.charAt(i) == '+' || s.charAt(i) == '-' ){
      if(sign) return false;
      else sign = true;
    }else if (!isDigit(s.charAt(i))){
      LOG_INF("%c\n", s.charAt(i));
      return false;
    }
  }
  return true;
}
bool isFloat(const std::string& str)
{
    if (str.empty())
        return false;

    char* ptr;
    strtof(str.c_str(), &ptr);
    return (*ptr) == '\0';
}
bool isInt(const std::string& str)
{
    if (str.empty())
        return false;

    char* ptr;
    strtol(str.c_str(), &ptr,10);
    return (*ptr) == '\0';
}

// Reset timer used for sleep
void ResetCountdownTimer(){
  sleepTimerCountdown = SLEEP_DELAY_INTERVAL; 
}

// Remove all ini files 
void reset(){
  LOG_DBG("Removing ini files.");
  listDir("/", 1);
  lastBoot.deleteConfig(LAST_BOOT_CONF);
  conf.deleteConfig();
}

//Logging to file or serial
#define MAX_OUT 200
static char fmtBuf[MAX_OUT];
static char outBuf[MAX_OUT];
static va_list arglist;

void logPrint(const char *format, ...) {
  strncpy(fmtBuf, format, MAX_OUT);
  fmtBuf[MAX_OUT - 1] = 0;
  va_start(arglist, format); 
  vsnprintf(outBuf, MAX_OUT, fmtBuf, arglist);
  va_end(arglist);
  size_t msgLen = strlen(outBuf);
  Serial.print(outBuf);
  if (logFile) 
    logF.print(outBuf);  
}