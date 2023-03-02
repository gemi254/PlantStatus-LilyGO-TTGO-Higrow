void clearMqttRetainMsg();

// Enter deep sleep
void goToDeepSleep(const char *reason, bool error=true)
{
  clearMqttRetainMsg();

  if(onPower && error){
    LOG_INF("OnPower, no sleep on error: %s\n", reason);
    return;
  } 
  mqttClient.disconnect();
  
  //Disable wifi
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  uint16_t sleepTime = (error)?conf["sleep_time_error"].toInt() : conf["sleep_time"].toInt();

  //Configure the timer to wake us up!
  esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_S_FACTOR);
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
  
  //Remember last boot vals
  if(error) lastBoot.put("boot_cnt_err", String(lastBoot["boot_cnt_err"].toInt() + 1), true);
  else lastBoot.put("boot_cnt", String(lastBoot["boot_cnt"].toInt() + 1), true);
  lastBoot.put("sleep_reason", String(reason), true);
  lastBoot.put("bat_voltage", String(data.batVolt,2), true);
  lastBoot.put("bat_perc", String(data.batPerc,0), true); 
  //Save last boot vars
  lastBoot.saveConfigFile(LAST_BOOT_CONF);

  LOG_INF("Sleep, ms: %lu, reason: %s, Boots: %u, BootsErr: %u, millis: %lu\n", sleepTime, reason, lastBoot["boot_cnt"].toInt(), lastBoot["boot_cnt_err"].toInt(), (millis() - appStart));
  Serial.flush();
  if(dbgLog) dbgLog.close();  
  delay(100);
  //Go to sleep! Zzzz
  esp_deep_sleep_start();
}