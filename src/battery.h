// Calc battery state. Higrow uses TP4054 linear charger
uint32_t readBatteryADC(){
  uint32_t adc_reading = analogReadAvg(BAT_ADC, BATT_NO_OF_SAMPLES);
  return adc_reading;
}

float calcBattery(uint16_t AdcVolt){
  data.batAdcVolt = AdcVolt;
  // TP4054 Standalone Linear Li-lon Battery Charger
  // Preset 4.2V Charge Voltage with 1% Accuracy, 2.9V Trickle Charge Threshold
  // max adc: 2074 min adc: 1534

  //Battery adc readings limits
  float bat_reading_low = atof(conf("bat_adc_low").c_str());
  float bat_reading_high = atof(conf("bat_adc_high").c_str());

  //Battery voltage limits
  float bat_volt_low = atof(conf("bat_reading_low").c_str());
  float bat_volt_high = atof(conf("bat_reading_high").c_str());

  float battery_voltage =  ( bat_volt_high - bat_volt_low ) / ( bat_reading_high - bat_reading_low ) * AdcVolt
        +  bat_volt_high - bat_reading_high * ( bat_volt_high - bat_volt_low ) / ( bat_reading_high - bat_reading_low ) ;//true volts

  data.batVolt = battery_voltage;
  //Battery volt percent
  float batPerc = 100.0F * ( battery_voltage - bat_volt_low ) / ( bat_volt_high - bat_volt_low);
  //Average from previous value
  float batPercAgv = batPerc;
  if(lastBoot["bat_perc"] !="" && lastBoot["bat_perc"] !="nan"){
    float lastPerc = (atof(lastBoot["bat_perc"].c_str()));
    batPercAgv = ((lastPerc + truncateFloat(batPerc, 0) ) / 2.0F);
  }
  LOG_D("Battery last perc: %s, cur: %3.1f, avg: %3.1f\n", lastBoot["bat_perc"].c_str(), batPerc, batPercAgv);

  if (batPerc > BATT_PERC_ONPOWER) onPower = true;
  else onPower = false;
  LOG_D("Battery adcV: %lu, V: %3.3f, Avg perc: %3.1f %%, onPower: %i\n", AdcVolt, battery_voltage, batPerc, onPower);
  return batPercAgv;

}
// Calculate battery discharge days
// Connected to the Internet about 150mA of current, the peak 300mA, and the average 100mA
float calcBatteryDays(){
  float daysOnBattery = 0.0F;
  if (onPower){
    String curDate = getCurDateTimeString();
    //Is time synced?
    if(curDate.length()>0){
      data.batChargeDate = curDate;
      File f = STORAGE.open(LAST_BAT_INI, "w");
      f.print(data.batChargeDate);
      f.close();
    }
    //Reset counters
    lastBoot.put("boot_cnt", "0",true);
    lastBoot.put("boot_cnt_err", "0",true);
    lastBoot.put("last_error", "",true);
    LOG_D("Battery CHARGING, date: %s\n", curDate.c_str());
  }else{ // Discharging
    daysOnBattery = 0.0F;
    File f = STORAGE.open(LAST_BAT_INI,"r");
    if(f){
      data.batChargeDate = f.readString();
      f.close();
      time_t tmStart = convertDateTimeString(data.batChargeDate);
      time_t tmNow = getEpoch();
      if(tmStart > 10000L && tmNow > 10000L){
        daysOnBattery  = (tmNow - tmStart) / BATT_CHARGE_DATE_DIVIDER;
        if(daysOnBattery < 0.0F)  daysOnBattery = 0.0F;
      }
      //LOG_I("Battery DISCHARGING, st: %lu, now: %lu\n", tmStart, tmNow);
    }else{ //Bat file not exists
      File f = STORAGE.open(LAST_BAT_INI, "w");
      f.print(getCurDateTimeString());
      f.close();
    }
    LOG_D("Battery DISCHARGING, date: %s, days: %5.3f\n", data.batChargeDate.c_str(), daysOnBattery);
  }

  return daysOnBattery;
}