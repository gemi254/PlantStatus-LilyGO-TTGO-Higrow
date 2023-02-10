// Initialize on board sensors according config
void initSensors(){
  //Temp/hum sensors
  if(conf["dht_type"]=="DHT11" || conf["dht_type"]=="DHT21" || conf["dht_type"]=="DHT22"){      
      uint8_t dht_type = 11;
      if(conf["dht_type"]=="DHT11") dht_type = 11;
      else if(conf["dht_type"]=="DHT21") dht_type = 21;
      else if(conf["dht_type"]=="DHT22") dht_type = 22;
      LOG_INF("Init DHT sensor on pin: %i, type: %s\n",DHT_PIN, conf["dht_type"]);
      pDht = new DHT(DHT_PIN, dht_type);
      pDht->begin();
      dhtFound = true;
  }else{
      bool wireOk = Wire.begin(I2C_SDA, I2C_SCL); // wire can not be initialized at beginng, the bus is busy
      if (wireOk){
        LOG_INF("Wire begin ok\n");
        pBmp = new Adafruit_BME280();
        if (!pBmp->begin()){
          LOG_ERR("Could not find a valid BMP280 sensor, check wiring!");
        }else{
          bmeFound = true;
        }
      }else{
        LOG_ERR("Wire failed to begin\n");
      } 
  }
 
  //Light sensor
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)){
    LOG_INF("BH1750 begin\n");
  }else{
    LOG_ERR("Error initialising BH1750\n");
  }
}

// READ Salt
uint32_t readSalt()
{
  uint8_t samples = 120;
  uint32_t humi = 0;
  uint16_t array[120];

  for (int i = 0; i < samples; i++)
  {
    array[i] = analogRead(SALT_PIN);
    delay(2);
  }
  std::sort(array, array + samples);
  for (int i = 0; i < samples; i++)
  {
    if (i == 0 || i == samples - 1)
      continue;
    humi += array[i];
  }
  humi /= samples - 2;
  LOG_DBG("Salt read: %lu\n", humi);
  return humi;
}

// Read and calibrate Soil
uint8_t readSoil(){  
  uint16_t soil = analogRead(SOIL_PIN);
  uint8_t soilM = map(soil, conf["soil_min"].toInt(), conf["soil_max"].toInt(), 100, 0);
  LOG_DBG("Soil read: %lu, map: %lu\n",soil ,soilM);  
  return soilM;
}

float readSoilTemp()
{
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
String getLogFileName(bool createDir = true){
  if(!timeSynchronized) return "";
  String dirName = getCurDateTimeString(true,false);
  String fileName = getCurDateTimeString(false,true);
  if(createDir) STORAGE.mkdir(dirName);
  return String(DATA_DIR) + dirName + fileName + ".csv";
}
//Log sensors to a csv file on storage
void logSensors(){
  if(!timeSynchronized) return;
  String sep="\t";  
  String fullPath =getLogFileName();
  String line = "";
  if(!STORAGE.exists(fullPath)){
    LOG_DBG("New log file: %s\n", fullPath.c_str());
    line += "DateTime               " + sep;
    line += "temp" + sep;
    line += "humid" + sep;
    line += "pressr" + sep;
    line += "lux" + sep;
    line += "soil" + sep;
    line += "salt" + sep;
    line += "batPerc";
    line +="\n";
  }
  line += data.time + sep;
  line += String(data.temp + atof(conf["offs_temp"].c_str()), 1) + sep;
  line += String(data.humid + atof(conf["offs_humid"].c_str()), 1) + sep;
  line += String(data.pressure + atof(conf["offs_pressure"].c_str()), 1) + sep;
  line += String(data.lux + atof(conf["offs_lux"].c_str()), 1) + sep;
  line += (data.soil + conf["offs_soil"].toInt()) + sep;
  line += (data.salt + conf["offs_salt"].toInt()) + sep;
  line += String(data.batPerc, 0);
  line +="\n";
  writeFile(fullPath.c_str(), line.c_str());
  LOG_DBG("Log to: %s, line: %s", fullPath.c_str(),line.c_str() );
}

// Read on board sensors
void readSensors(){
  float luxRead = lightMeter.readLightLevel(); // 1st read seems to return 0 always
  LOG_DBG("Lux first read: %4.1f\n", luxRead);  
  //delay(2000);

  if(dhtFound){
    float t12 = pDht->readTemperature(); // Read temperature as Fahrenheit then dht.readTemperature(true)
    data.temp = t12;
    delay(500);
    float h12 = pDht->readHumidity();
    data.humid = h12;
  }else if (bmeFound) {
    float bme_temp = pBmp->readTemperature();
    if( isnan(bme_temp) || bme_temp < -40.0F || bme_temp > 85.0F ) bme_temp = 0.0;
    data.temp = bme_temp;
    float bme_humid = pBmp->readHumidity();
    if( isnan(bme_humid) || bme_humid < -40.0F || bme_humid > 85.0F ) bme_humid = 0.0;
    data.humid = bme_humid;
    float bme_pressure = (pBmp->readPressure() / 100.0F);
    if( isnan(bme_pressure) || bme_pressure < 300.0F || bme_pressure > 1100.0F ) bme_pressure = 0.0;
    data.pressure = bme_pressure;
  }

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
  data.batPerc  = calcBattery(adcVolt);
  data.batDays = calcBatteryDays();

  //Correction
  if(data.batPerc > 100.0F) data.batPerc = 100.0F;

  luxRead = lightMeter.readLightLevel();
  LOG_DBG("Lux read: %4.1f\n", luxRead);
  if (isnan(luxRead) || luxRead < 0.0 || luxRead > 54612.5F) luxRead = 0.0;
  data.lux = luxRead;

  //conf.saveConfigFile(CONF_FILE);
  
  //Get last boot values
  data.time = getCurDateTimeString();
  data.bootCnt = lastBoot["boot_cnt"].toInt();
  data.bootCntError = lastBoot["boot_cnt_err"].toInt();
  data.sleepReason = lastBoot["sleep_reason"];
  
  //Append to log
  logSensors();

  //Next auto reading reset
  sensorReadMs = millis();
}
