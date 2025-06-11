bool initDHTsensor(){
  uint8_t dht_type = conf("dht_type").toInt();
  LOG_D("Init DHT sensor on pin: %i, type: %i\n",DHT_PIN, dht_type);
  pDht = new DHT(DHT_PIN, dht_type);
  pDht->begin();
  int i = 5;
  while (i--) {
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    //float t = pDht->readTemperature(false);
    float h = pDht->readHumidity(true);
    if (isnan(h)) {
      LOG_E("DHT sensor fail, type: %i\n", dht_type);
      // Wait a few seconds between measurements.
      delay(500);
    }else{
      LOG_D("Probe DHT sensor h: %3.1f\n",h);
      //LOG_E("Probe DHT sensor t: %3.1f, h: %3.1f\n",t, h);
      return true;
    }
  }
  return false;
}

bool initBME280(){
  if (wireOk){
    LOG_I("Wire begin OK\n");
    pBmp = new Adafruit_BME280();
    if (!pBmp->begin()){
      LOG_E("BME280 begin error\n");
      return false;
    }else{
      return true;
    }
  }else{
    LOG_E("Wire begin error\n");
    return false;
  }
}
bool initBH1750(){
  //Light sensor
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)){
    LOG_D("BH1750 begin OK\n");
  }else{
    LOG_E("BH1750 begin error \n");
    return false;
  }
  return true;
}
// Initialize on board sensors according config
bool initSensors(){
  // Light sensor
  initBH1750();

  //Temp/hum sensors
  if(conf("dht_type")=="11" || conf("dht_type")=="21" || conf("dht_type")=="22"){
    dhtFound = initDHTsensor();
  }else{
    bmeFound = initBME280();
  }

  return true;
}

// READ Salt
uint32_t readSalt(){
  uint32_t salt = analogReadAvg(SALT_PIN, SALT_NO_OF_SAMPLES);
  LOG_D("Read salt: %lu\n", salt);
  return salt;
}

// Read and calibrate Soil moisture
uint32_t readSoil(){

  uint32_t soil = analogReadAvg(SOIL_PIN, SOIL_NO_OF_SAMPLES);

  // Auto adjust min max
  if(conf("auto_adjust_soil").toInt()){
    bool save = false;
    if(soil < conf("soil_min").toInt()){
      //conf.put("soil_min", soil);
      conf["soil_min"] = soil;
      LOG_I("Adjust soil min: %lu\n", soil);
      save = true;
    }
    if(soil > conf("soil_max").toInt()){
      //conf.put("soil_max", soil);
      conf["soil_max"] = soil;
      LOG_I("Adjust soil max: %lu\n", soil);
      save = true;
    }
    if(save) conf.saveConfigFile();

  }
  uint8_t soilM = map(soil, conf("soil_min").toInt(), conf("soil_max").toInt(), 100, 0);
  LOG_D("Read soil: %lu, map: %lu\n",soil ,soilM);
  return soilM;
}


float readSoilTemp(){
  float temp;
  //READ Soil Temperature
  #ifdef USE_18B20_TEMP_SENSOR
    //Single data stream upload
    temp = temp18B20.temp();
  #else
    temp = 0.00;
  #endif
  return temp;
}

#ifdef USE_AUTO_WATER
void WateringCallback(bool value)
{
    LOG_D("motorButton Triggered: %s\n", (value) ? "true" : "false");
    digitalWrite(MOTOR_PIN, value);
#ifdef USE_ADAFRUIT_NEOPIXEL
    pixels->setPixelColor(0, value ? 0x00FF00 : 0);
    pixels->show();
#endif    
    //motorButton->update(value);
}
#endif  /*__HAS_MOTOR__*/
/*
After the measurement Time register value is changed according to the result:
lux > 40000 ==> MTreg =  32
lux < 40000 ==> MTreg =  69  (default)
lux <    10 ==> MTreg = 138
*/
float readLightValue(){
    float lux = lightMeter.readLightLevel();
    #ifdef LUX_AUTOAJUST
      if (lux < 0){ //-1 : no valid return value -2 : sensor not configured
          LOG_E("Failed to read BH1750!\n");
          return NAN;
      }else{
        byte MTreg = 69;
        if (lux > 40000.0F) // reduce measurement time - needed in direct sun light
          MTreg = 32;
        else if (lux > 10.0F)  // typical light environment
          MTreg = 69;
        else if (lux <= 10.0F)  //very low light environment
          MTreg = 138;

        if(MTreg!=69){ //Default value
          if (!lightMeter.setMTreg(MTreg)){
            LOG_E("Failed to set BH1750 mtreg: %i\n", MTreg);
          }else{
            LOG_D("Adjusted BH1750 mtreg: %i\n", MTreg);
            lux = lightMeter.readLightLevel();
            delay(150);
          }
        }
    # endif
    LOG_D("Read lux : %4.1f\n", lux);
    if (isnan(lux) || lux < 0.0 || lux > 54612.5F) lux = NAN;
    return lux;
  }
}

void readDHTSensor(){
  float temp = NAN;
  float humd = NAN;
  int i = 5;
  while (i--) {
    temp = pDht->readTemperature(false); // Read temperature as Fahrenheit then dht.readTemperature(true)
    humd = pDht->readHumidity();
    if(!isnan(temp) && !isnan(humd)){
      i = 0;
    }else{
      LOG_D("Probe DHT i: %i, temp: %4.1f, hum: %4.1f\n", i, temp, humd);
      delay(500);
    }
  }
  if( isnan(temp) || temp < -40.0F || temp > 85.0F ) temp = NAN;
  if( isnan(humd) || humd < 0 || humd > 100.0F ) humd = NAN;
  data.temp = temp;
  data.humid = humd;
  float hic = pDht->computeHeatIndex(temp, humd, false);
  LOG_D("Read DHT temp: %4.1f, hum: %4.1f, Heat ndx: %4.1f\n", temp, humd, hic);
}

void readBmpSensor(){
  float bme_temp = pBmp->readTemperature();
  if( isnan(bme_temp) || bme_temp < -40.0F || bme_temp > 85.0F ) bme_temp = NAN;
  data.temp = bme_temp;
  float bme_humid = pBmp->readHumidity();
  if( isnan(bme_humid) || bme_humid < 0 || bme_humid > 100.0F ) bme_humid = NAN;
  data.humid = bme_humid;
  float bme_pressure = (pBmp->readPressure() / 100.0F);
  if( isnan(bme_pressure) || bme_pressure < 300.0F || bme_pressure > 1100.0F ) bme_pressure = NAN;
  data.pressure = bme_pressure;
  LOG_D("Read bme280, temp: %4.1f, hum: %4.1f, press: %4.1f\n", bme_temp, bme_humid, bme_pressure);
}
// Read on board sensors
void readSensors(){
  if(dhtFound)         readDHTSensor();
  else if (bmeFound)   readBmpSensor();

  uint8_t  soil = readSoil();
  data.soil = soil;
  /*
  float soilTemp = readSoilTemp();
  data.soilTemp = soilTemp;
  */
  uint32_t salt = readSalt();
  data.salt = (uint8_t)salt;
  //data.saltadvice = getSaltAdvice(salt);

  //Battery status, and charging status and days.
  data.batPerc = truncateFloat(calcBattery(adcVolt), 0);
  data.batDays = calcBatteryDays();

  //Correction
  if(data.batPerc > 100.0F) data.batPerc = 100.0F;
  else if(data.batPerc < 0.0F) data.batPerc = 0.0F;

  //Lux level
  data.lux = readLightValue();

  //conf.saveConfigFile(CONF_FILE);
  //Get last boot values
  data.time = getCurDateTimeString();
  data.bootCnt = lastBoot["boot_cnt"].toInt();
  data.bootCntError = lastBoot["boot_cnt_err"].toInt();
  data.sleepReason = lastBoot["sleep_reason"];
  data.lastError = (lastBoot["last_error"]=="") ? "none" : lastBoot["last_error"];

#ifdef USE_AUTO_WATER
  if (data.soil < WATERING_MIN_SOIL) {
      LOG_D("Start adding water");
      WateringCallback(true);
  }
  if (data.soil >= WATERING_MAX_SOIL) {
      LOG_D("Stop adding water");
      WateringCallback(false);
  }
#endif  /*USE_AUTO_WATER*/

  //Next auto reading reset
  sensorReadMs = millis();
}

// Get a log filename
String getLogFileName(bool createDir = true){
  if(!timeSynchronized) return "";
  String dirName = getCurDateTimeString(true,false);
  String fileName = getCurDateTimeString(false,true);
  if(createDir) STORAGE.mkdir(dirName);
  return String(DATA_DIR) + dirName + fileName + ".csv";
}

//Log sensors to a csv file on storage
void logSensors(){
  if(!timeSynchronized){
    LOG_E("Time is not sync\n");
    return;
  }
  String sep="\t";
  String fullPath = getLogFileName();
  String line = "";
  if(!STORAGE.exists(fullPath)){
    LOG_D("New log file: %s\n", fullPath.c_str());
    line += "DateTime               " + sep;
    line += "temp" + sep;
    line += "humid" + sep;
    if(conf("dht_type")=="BMP280")
      line += "pressr" + sep;
    line += "lux" + sep;
    line += "soil" + sep;
    line += "salt" + sep;
#ifdef VERBOSE_MODE
    line += "batAdc" + sep;
#endif
    line += "batPerc";
    line +="\n";
  }
  line += data.time + sep;
  line += String(data.temp + atof(conf("offs_temp").c_str()), 1) + sep;
  line += String(data.humid + atof(conf("offs_humid").c_str()), 1) + sep;
  if(conf("dht_type")=="BMP280")
    line += String(data.pressure + atof(conf("offs_pressure").c_str()), 1) + sep;
  line += String(data.lux + atof(conf("offs_lux").c_str()), 1) + sep;
  line += (data.soil + conf("offs_soil").toInt()) + sep;
  line += (data.salt + conf("offs_salt").toInt()) + sep;
  #ifdef VERBOSE_MODE
    line += String(data.batAdcVolt) + sep;
  #endif
  line += String(data.batPerc, 0);
  line +="\n";
  writeFile(fullPath.c_str(), line.c_str());
  LOG_I("Log: %s",line.c_str() );
}
