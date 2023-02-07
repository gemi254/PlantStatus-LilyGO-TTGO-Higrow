// Calc battery state
float calcBattery(uint16_t AdcVolt){
  // Battery adc volt measured with multimetr, after disconecting from board,
  // and value reported at end discharge, and value reported just after charging
  uint16_t bat_reading_low = 1551;
  uint16_t bat_reading_high = 2370;

  data.batAdcVolt = AdcVolt;
  // int vref = 1100;
  // float battery_voltage = ((float)volt / 4095.0) * 2.0 * 3.3 * (vref) / 1000;
  // float battery_voltage =  0.001338217338 * volt +  0.948424908425 ;//volts
  float bat_volt_low = atof(conf["bat_reading_low"].c_str());
  float bat_volt_high = atof(conf["bat_reading_high"].c_str());
  float battery_voltage =  ( bat_volt_high - bat_volt_low ) / ( bat_reading_high - bat_reading_low ) * AdcVolt
        +  bat_volt_high - bat_reading_high * ( bat_volt_high - bat_volt_low ) / ( bat_reading_high - bat_reading_low ) ;//true volts

  data.batVolt = battery_voltage;
  //Battery volt percents
  float batPerc = 100.0F * ( battery_voltage - bat_volt_low ) / ( bat_volt_high - bat_volt_low);
  batPerc = truncateFloat(batPerc, 1);
  LOG_INF("Battery ADC volt: %lu, volt: %3.3f, perc: %3.1f %%\n", AdcVolt, battery_voltage, batPerc);
  return batPerc;
  
}
// Calculate battery discharge days
float calcBatteryDays(){ 
  float daysOnBattery = 0.0F;
  if (data.batPerc > BATT_PERC_ONPOWER){    
    onPower = true;
    String curDate = getCurDateTimeString();
    //Is time synced?
    if(curDate.length()>0){
      data.batChargeDate = curDate;
      lastBoot.put("bat_charge_date", data.batChargeDate);
    }
    //Reset counters
    lastBoot.put("boot_cnt", "0");
    lastBoot.put("boot_cnt_err", "0");
    LOG_INF("Battery CHARGING, date: %s\n", curDate.c_str());    
  }else{ //Discharging
    onPower = false;
    data.batChargeDate = lastBoot["bat_charge_date"];
    time_t tmStart = convertDateTimeString(data.batChargeDate);
    time_t tmNow = getEpoch();
    if(tmStart > 10000L && tmNow > 10000L){
      daysOnBattery  = (tmNow - tmStart) / BATT_CHARGE_DATE_DIVIDER;
      if(daysOnBattery < 0.0F)  daysOnBattery = 0.0F;      
    }else{
      daysOnBattery = 0.0F;
    }
    LOG_INF("Battery DISCHARGING, st: %lu, now: %lu, date: %s, days: %5.3f\n", tmStart, tmNow, data.batChargeDate.c_str(), daysOnBattery);
  }
  
  return daysOnBattery;
}