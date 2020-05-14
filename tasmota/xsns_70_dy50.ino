/*
  xsns_70_dy50.ino - Support for dy50 fingerprint reader on Tasmota

  Copyright (C) 2020

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_DY50

#define XSNS_70                                     70

#include <TasmotaSerial.h>
#include <Adafruit_Fingerprint.h>

TasmotaSerial *DY50_Serial;
Adafruit_Fingerprint *finger;

uint8_t dy50_scantimer = 0;      // Used to prevent multiple successful reads within 2 second window
uint8_t dy50_pin_touch = 0;         
uint8_t dy50_function = 0;
uint8_t dy50_newdata_id = 0;
uint8_t dy50_deldata_id = 0;
bool dy50_active = false;

void DY50_Init(void)
{
  if (PinUsed(GPIO_DY50_RX) && PinUsed(GPIO_DY50_TX) && PinUsed(GPIO_DY50_TOUCH)) 
  {
    DY50_Serial = new TasmotaSerial(Pin(GPIO_DY50_RX), Pin(GPIO_DY50_TX), 1);
    
    if (DY50_Serial->begin(115200)) 
    {
      
      if (DY50_Serial->hardwareSerial()) { ClaimSerial(); }
      DY50_Serial->flush();

      finger = new Adafruit_Fingerprint(DY50_Serial);
      //finger->begin(115200);

      delay(20);
      if (finger->verifyPassword()) 
      {
          finger->getTemplateCount();
          dy50_pin_touch = Pin(GPIO_DY50_TOUCH);
          pinMode(dy50_pin_touch, INPUT_PULLUP);
          finger->ledOff();
          dy50_active = true;
          AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader detected! Sensor contains %u templates!", finger->templateCount);
      }  
      
    
    }
    
  }
}

void DY50_ScanForFinger(void)
{
  if(dy50_active)
  {
    if(!digitalRead(dy50_pin_touch))
      {
        finger->ledOn();
        if(DY50_GetFingerprintID() > 0)
        {
          AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - found ID #%u with confidence of %u!", finger->fingerID,  finger->confidence);
          #ifdef USE_DOMOTICZ
              char finger_chr[FLOATSZ];
              dtostrfd(finger->fingerID  + finger->confidence / 100000.0, 5, finger_chr);
              DomoticzSensor(DZ_CURRENT, finger_chr);  // Send data as Domoticz Current value
          #endif  // USE_DOMOTICZ
        }
      }  
      else
      {
        finger->ledOff();
      }  
    switch(dy50_function)
    {
      case 3:
        finger->deleteModel(dy50_deldata_id);
        AddLog_P2(LOG_LEVEL_DEBUG,"FPR: DY50 Fingerprint Reader - Deleted #%u!", dy50_deldata_id);
        dy50_deldata_id = 0;
        break;
      case 2:
        DY50_EnrollNewFinger();
        break;
      case 1:
        finger->emptyDatabase();
        AddLog_P2(LOG_LEVEL_DEBUG,"FPR: DY50 Fingerprint Reader - Cleared DB!");
        break;
      case 0:
      default:
        dy50_scantimer = 7; // Ignore Fingers found for two seconds
        break;
    }
    dy50_function = 0;
  }
}

int8_t DY50_GetFingerprintID() {
  uint8_t p = finger->getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger->image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger->fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;
  
  // found a match!
  return finger->fingerID; 
}

uint8_t DY50_EnrollNewFinger()
{
  int p = -1;
  uint8_t counter = 100;

  finger->ledOn();

  AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Waiting for valid finger to enroll as #%u!", dy50_newdata_id);
  while (p != FINGERPRINT_OK && counter > 1) {
    p = finger->getImage();
    switch (p) {
    case FINGERPRINT_OK:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      AddLog_P2(LOG_LEVEL_DEBUG,"FPR: DY50 Fingerprint Reader - Waiting");
      counter --;
      delay(100);
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_IMAGEFAIL:
    default:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Error while Scanning - Code #%u!", p);
      return p;
    }
  }

  if(p != FINGERPRINT_OK)
  {
    AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Timeout while Scanning!");
    return FINGERPRINT_NOFINGER;
  }

  // OK success!
  p = finger->image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Image converted!");
      break;
    case FINGERPRINT_IMAGEMESS:
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
    default:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Error while Converting - Code #%u!", p);
      return p;
  }
  
  AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Remove finger!");
  delay(2000);

  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger->getImage();
  }

  p = -1;
  counter = 100;
  AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Waiting for valid finger to enroll as #%u again!", dy50_newdata_id);
  while (p != FINGERPRINT_OK && counter > 1) {
    p = finger->getImage();
    switch (p) {
    case FINGERPRINT_OK:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      AddLog_P2(LOG_LEVEL_DEBUG,"FPR: DY50 Fingerprint Reader - Waiting");
      counter --;
      delay(100);
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_IMAGEFAIL:
    default:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Error while Scanning - Code #%u!", p);
      return p;
    }
  }

  if(p != FINGERPRINT_OK)
  {
    AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Timeout while Scanning!");
    return FINGERPRINT_NOFINGER;
  }

  // OK success!
  p = finger->image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Image converted!");
      break;
    case FINGERPRINT_IMAGEMESS:
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
    default:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Error while Converting - Code #%u!", p);
      return p;
  }
  
  // OK converted!

  AddLog_P2(LOG_LEVEL_DEBUG,"FPR: DY50 Fingerprint Reader - Creating model for #%u", dy50_newdata_id);
  p = finger->createModel();
  switch (p) {
    case FINGERPRINT_OK:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Prints matched!");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_ENROLLMISMATCH:
    default:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Error while creating model - Code #%u!", p);
      return p;
  }   
  
  AddLog_P2(LOG_LEVEL_DEBUG,"FPR: DY50 Fingerprint Reader - Store model for #%u", dy50_newdata_id);
  p = finger->storeModel(dy50_newdata_id);
  switch (p) {
    case FINGERPRINT_OK:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Prints stored!");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
    case FINGERPRINT_BADLOCATION:
    case FINGERPRINT_FLASHERR:
    default:
      AddLog_P2(LOG_LEVEL_INFO,"FPR: DY50 Fingerprint Reader - Error while storing model - Code #%u!", p);
      return p;
  }  

}

bool DY50_Command(void)
{
  bool serviced = true;
  uint8_t paramcount = 0;
  if (XdrvMailbox.data_len > 0) {
    paramcount=1;
  } else {
    serviced = false;
    return serviced;
  }
  char sub_string[XdrvMailbox.data_len];
  char sub_string_tmp[XdrvMailbox.data_len];
  for (uint32_t ca=0;ca<XdrvMailbox.data_len;ca++) {
    if ((' ' == XdrvMailbox.data[ca]) || ('=' == XdrvMailbox.data[ca])) { XdrvMailbox.data[ca] = ','; }
    if (',' == XdrvMailbox.data[ca]) { paramcount++; }
  }
  UpperCase(XdrvMailbox.data,XdrvMailbox.data);

  if (!strcmp(subStr(sub_string, XdrvMailbox.data, ",", 1),"E")) {
    dy50_function = 1; // empty Template Database ...
    AddLog_P(LOG_LEVEL_INFO, PSTR("FPR: DY50 Fingerprint Reader - database is cleared"));
    ResponseTime_P(PSTR(",\"DY50\":{\"COMMAND\":\"E\"}}"));
    return serviced;
  }

  if (!strcmp(subStr(sub_string, XdrvMailbox.data, ",", 1),"S")) {
    if (paramcount > 1) {
      if (XdrvMailbox.data[XdrvMailbox.data_len-1] == ',') {
        serviced = false;
        return serviced;
      }
      sprintf(sub_string_tmp,subStr(sub_string, XdrvMailbox.data, ",", 2));

      dy50_newdata_id = (!strlen(sub_string_tmp)) ? 0 : atoi(sub_string_tmp);
      if (dy50_newdata_id == 0 || dy50_newdata_id > 127) 
      { 
        serviced = false; 
        return serviced; 
      }
      dy50_function = 2;
      AddLog_P2(LOG_LEVEL_INFO, PSTR("FPR: DY50 Fingerprint Reader - Next scanned finger will be stored at #%u"), dy50_newdata_id);
      ResponseTime_P(PSTR(",\"DY50\":{\"COMMAND\":\"S\"}}"));
      
      return serviced;
    }
  }

  if (!strcmp(subStr(sub_string, XdrvMailbox.data, ",", 1),"D")) {
    if (paramcount > 1) {
      if (XdrvMailbox.data[XdrvMailbox.data_len-1] == ',') {
        serviced = false;
        return serviced;
      }
      sprintf(sub_string_tmp,subStr(sub_string, XdrvMailbox.data, ",", 2));

      dy50_deldata_id = (!strlen(sub_string_tmp)) ? 0 : atoi(sub_string_tmp);
      if (dy50_deldata_id == 0 || dy50_deldata_id > 127) 
      { 
        serviced = false; 
        return serviced; 
      }
      dy50_function = 3;
      AddLog_P2(LOG_LEVEL_INFO, PSTR("FPR: DY50 Fingerprint Reader - Deleted ID #%u"), dy50_deldata_id);
      ResponseTime_P(PSTR(",\"DY50\":{\"COMMAND\":\"D\"}}"));
      return serviced;
    }
  }
}

bool Xsns70(uint8_t function)
{
  bool result = false;

  switch (function) {
    case FUNC_INIT:
      DY50_Init();
      result = true;
      break;
    case FUNC_EVERY_50_MSECOND:
      break;
    case FUNC_EVERY_100_MSECOND:
      break;
    case FUNC_EVERY_250_MSECOND:
      if (dy50_scantimer > 0) {
        dy50_scantimer--;
      } else {
        DY50_ScanForFinger();
      }
      break;
    case FUNC_EVERY_SECOND:
      break;
    case FUNC_COMMAND_SENSOR:
      if (XSNS_70 == XdrvMailbox.index) {
        result = true;
        result = DY50_Command();
      }
      break;
  }
  return result;
}

#endif // USE_DY50
