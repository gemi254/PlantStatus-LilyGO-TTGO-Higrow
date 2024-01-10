// Initialize on board sensors according config
bool initSensors(){
  //Temp/hum sensors
  if(conf["dht_type"]=="DHT11" || conf["dht_type"]=="DHT21" || conf["dht_type"]=="DHT22"){      
      uint8_t dht_type = 11;
      if(conf["dht_type"]=="DHT11") dht_type = 11;
      else if(conf["dht_type"]=="DHT21") dht_type = 21;
      else if(conf["dht_type"]=="DHT22") dht_type = 22;
      LOG_I("Init DHT sensor on pin: %i, type: %s\n",DHT_PIN, conf["dht_type"]);
      pDht = new DHT(DHT_PIN, dht_type);
      pDht->begin();
      dhtFound = true;
  }else{
      bool wireOk = Wire.begin(I2C_SDA, I2C_SCL); // wire can not be initialized at beginng, the bus is busy
      if (wireOk){
        LOG_I("Wire begin OK\n");
        pBmp = new Adafruit_BME280();
        if (!pBmp->begin()){
            LOG_E("BME280 begin error\n");
            return false;
        }else{
          bmeFound = true;
        }
      }else{
        LOG_E("Wire begin error\n");
        return false;
      } 
  }
 
  //Light sensor
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)){
    LOG_I("BH1750 begin OK\n");
  }else{
    LOG_E("BH1750 begin error \n");
    return false;
  }
  return true;
}

// READ Salt
uint32_t readSalt(){
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
  LOG_D("Read salt: %lu\n", humi);
  return humi;
}

// Read and calibrate Soil
uint8_t readSoil(){  
  uint32_t adc_reading = 0;    
  for (int i = 0; i < SOIL_NO_OF_SAMPLES; i++) {
      adc_reading += analogRead(SOIL_PIN);
      delay(2);
  }
  uint16_t soil = adc_reading / SOIL_NO_OF_SAMPLES;
  uint8_t soilM = map(soil, conf["soil_min"].toInt(), conf["soil_max"].toInt(), 100, 0);
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
          LOG_I("Adjusted BH1750 mtreg: %i\n", MTreg);   
          lux = lightMeter.readLightLevel();
          delay(150);
        }  
      }    
    # endif
    LOG_D("Read lux : %4.1f\n", lux);
    return lux;
  }
}
// Read on board sensors
void readSensors(){
  //float luxRead = lightMeter.readLightLevel(); // 1st read seems to return 0 always
  if(dhtFound){
    float t12 = pDht->readTemperature(); // Read temperature as Fahrenheit then dht.readTemperature(true)
    data.temp = t12;
    float h12 = pDht->readHumidity(true);
    data.humid = h12;
    float hic = pDht->computeHeatIndex(t12, h12, false);
    LOG_D("Read DHT, temp: %4.1f, hum: %4.1f, Heat ndx: %4.1f\n", t12, h12,hic);    
  }else if (bmeFound) {
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
  float lux = readLightValue();
  if (isnan(lux) || lux < 0.0 || lux > 54612.5F) lux = NAN;  
  data.lux = lux;

  //conf.saveConfigFile(CONF_FILE);  
  //Get last boot values
  data.time = getCurDateTimeString();
  data.bootCnt = lastBoot["boot_cnt"].toInt();
  data.bootCntError = lastBoot["boot_cnt_err"].toInt();
  data.sleepReason = lastBoot["sleep_reason"];

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
  String fullPath =getLogFileName();
  String line = "";
  if(!STORAGE.exists(fullPath)){
    LOG_D("New log file: %s\n", fullPath.c_str());
    line += "DateTime               " + sep;
    line += "temp" + sep;
    line += "humid" + sep;
    line += "pressr" + sep;
    line += "lux" + sep;
    line += "soil" + sep;
    line += "salt" + sep;
#ifdef DEBUG_BATTERY    
    line += "batAdc" + sep;
#endif    
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
  #ifdef DEBUG_BATTERY    
    line += String(data.batAdcVolt) + sep;
  #endif
  line += String(data.batPerc, 0);
  line +="\n";
  writeFile(fullPath.c_str(), line.c_str());
  LOG_D("Log to: %s\n, line: %s", fullPath.c_str(),line.c_str() );
}

