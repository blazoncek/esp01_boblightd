#include <stdint.h>
#include <math.h>
#include <Arduino.h>
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <StreamUtils.h>
#include <PrintEx.h>
#include <pgmspace.h>


#include "settings.h"


// strings (to reduce IRAM pressure)
static const char _CONFIG[] PROGMEM = "/config.conf";

void fillBobLights(int bottom, int left, int top, int right, float pct_scan)
{
/*
# boblight
# Copyright (C) Bob  2009 
#
# makeboblight.sh created by Adam Boeglin <adamrb@gmail.com>
#
# boblight is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# boblight is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

  int lightcount = 0;
  int total = top+left+right+bottom;
  int bcount;

  if ( total > WS2812_MAX_LEDS ) {
    #if DEBUG
    Serial.println(F("Too many lights."));
    #endif
    return;
  }
  
  if ( bottom > 0 )
  {
    bcount = 1;
    float brange = 100.0/bottom;
    float bcurrent = 50.0;
    if ( bottom < top )
    {
      int diff = top - bottom;
      brange = 100.0/top;
      bcurrent -= (diff/2)*brange;
    }
    while ( bcount <= bottom/2 ) 
    {
      float btop = bcurrent - brange;
      String name = "b"+String(bcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = btop;
      lights[lightcount].hscan[1] = bcurrent;
      lights[lightcount].vscan[0] = 100 - pct_scan;
      lights[lightcount].vscan[1] = 100;
      lightcount+=1;
      bcurrent = btop;
      bcount+=1;
    }
  }

  if ( left > 0 )
  {
    int lcount = 1;
    float lrange = 100.0/left;
    float lcurrent = 100.0;
    while (lcount <= left )
    {
      float ltop = lcurrent - lrange;
      String name = "l"+String(lcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = 0;
      lights[lightcount].hscan[1] = pct_scan;
      lights[lightcount].vscan[0] = ltop;
      lights[lightcount].vscan[1] = lcurrent;
      lightcount+=1;
      lcurrent = ltop;
      lcount+=1;
    }
  }

  if ( top > 0 )
  {
    int tcount = 1;
    float trange = 100.0/top;
    float tcurrent = 0;
    while ( tcount <= top )
    {
      float ttop = tcurrent + trange;
      String name = "t"+String(tcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = tcurrent;
      lights[lightcount].hscan[1] = ttop;
      lights[lightcount].vscan[0] = 0;
      lights[lightcount].vscan[1] = pct_scan;
      lightcount+=1;
      tcurrent = ttop;
      tcount+=1;
    }
  }

  if ( right > 0 )
  {
    int rcount = 1;
    float rrange = 100.0/right;
    float rcurrent = 0;
    while ( rcount <= right )
    {
      float rtop = rcurrent + rrange;
      String name = "r"+String(rcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = 100-pct_scan;
      lights[lightcount].hscan[1] = 100;
      lights[lightcount].vscan[0] = rcurrent;
      lights[lightcount].vscan[1] = rtop;
      lightcount+=1;
      rcurrent = rtop;
      rcount+=1;
    }
  }
  
        
  if ( bottom > 0 )
  {
    float brange = 100.0/bottom;
    float bcurrent = 100;
    if ( bottom < top )
    {
      brange = 100.0/top;
    }
    while ( bcount <= bottom )
    {
      float btop = bcurrent - brange;
      String name = "b"+String(bcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = btop;
      lights[lightcount].hscan[1] = bcurrent;
      lights[lightcount].vscan[0] = 100 - pct_scan;
      lights[lightcount].vscan[1] = 100;
      lightcount+=1;
      bcurrent = btop;
      bcount+=1;
    }
  }

  numLights = lightcount;

  #if DEBUG
  Serial.println(F("Fill light data: "));
  char tmp[64];
  sprintf(tmp, " lights %d\n", numLights);
  Serial.print(tmp);
  for (int i=0; i<numLights; i++)
  {
    sprintf(tmp, " light %s scan %2.1f %2.1f %2.1f %2.1f\n", lights[i].lightname, lights[i].vscan[0], lights[i].vscan[1], lights[i].hscan[0], lights[i].hscan[1]);
    Serial.print(tmp);
  }
  #endif

}

bool saveBobConfig()
{
  //read configuration from FS json
  if ( SPIFFS.begin() )
  {
    String conf_str = FPSTR(_CONFIG);
    String tmp_str;
    
    //file exists, reading and loading
    File configFile = SPIFFS.open(conf_str, "w");
    if ( configFile )
    {
      const size_t capacity = JSON_ARRAY_SIZE(WS2812_MAX_LEDS) + JSON_OBJECT_SIZE(2) + (WS2812_MAX_LEDS)*JSON_OBJECT_SIZE(5) + 90;
      DynamicJsonDocument doc(capacity);

      doc["lights"] = numLights;
      JsonArray lightdata = doc.createNestedArray("lightdata");
      for (int i=0; i<numLights; i++)
      {
        JsonObject light = lightdata.createNestedObject();
        if ( light.isNull() )
        {
          #if DEBUG
          Serial.print(F("Out of memory."));
          #endif
          break;
        }
//        else
//        {
//          #if DEBUG
//          Serial.println(lights[i].lightname);
//          #endif
//        }
        light["name"] = lights[i].lightname;
        light["h1"] = round(lights[i].hscan[0]*10)/10.0;
        light["h2"] = round(lights[i].hscan[1]*10)/10.0;
        light["v1"] = round(lights[i].vscan[0]*10)/10.0;
        light["v2"] = round(lights[i].vscan[1]*10)/10.0;
      }
      
      serializeJson(doc, configFile);
      configFile.close();

      #if DEBUG
      Serial.println(F("Save config: "));
      serializeJson(doc, Serial);
      Serial.println();
      #endif

      return true;
    }
    else
    {
      #if DEBUG
      Serial.println(F("Failed to create config file."));
      #endif
    }
  } else {
    #if DEBUG
    Serial.println(F("Failed to initialize SPIFFS."));
    #endif
  }
  //end save

  return false;
}

bool loadBobConfig()
{
  //read configuration from FS json
  if ( SPIFFS.begin() )
  {
    String conf_str = FPSTR(_CONFIG);
    String tmp_str;
    
    if ( SPIFFS.exists(conf_str) )
    {
      #if DEBUG
      Serial.println(F("Reading SPIFFS data."));
      #endif

      //file exists, reading and loading
      File configFile = SPIFFS.open(conf_str, "r");
      if ( configFile ) {
        size_t size = configFile.size();

        // Allocate a buffer to store contents of the file.
        char *buf = (char*)malloc(size+1);
        //std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf/*.get()*/, size);
        
        #if DEBUG
        Serial.println(size,DEC);
        Serial.println(buf/*.get()*/);
        #endif
        
        const size_t jsonsize = JSON_ARRAY_SIZE(WS2812_MAX_LEDS) + JSON_OBJECT_SIZE(2) + (WS2812_MAX_LEDS)*JSON_OBJECT_SIZE(5) + 90;
        #if DEBUG
        Serial.println(jsonsize,DEC);
        #endif
        DynamicJsonDocument doc(jsonsize);
        DeserializationError error = deserializeJson(doc, buf/*.get()*/);
        
        if ( !error && doc["lights"] != nullptr )
        {
          numLights = doc["lights"];
          for (int i=0; i<numLights; i++)
          {
            if ( doc["lightdata"][i] == nullptr )
            {
              numLights = i;
              break;
            }
            strncpy(lights[i].lightname, doc["lightdata"][i]["name"], 4);
            lights[i].hscan[0] = doc["lightdata"][i]["h1"];
            lights[i].hscan[1] = doc["lightdata"][i]["h2"];
            lights[i].vscan[0] = doc["lightdata"][i]["v1"];
            lights[i].vscan[1] = doc["lightdata"][i]["v2"];
          }

          #if DEBUG
          Serial.print(F("Config: "));
          char tmp[64];
          sprintf(tmp, "lights %d\n", numLights);
          Serial.print(tmp);
          for (int i=0; i<numLights; i++)
          {
            sprintf(tmp, "light %s scan %2.1f %2.1f %2.1f %2.1f\n", lights[i].lightname, lights[i].vscan[0], lights[i].vscan[1], lights[i].hscan[0], lights[i].hscan[1]);
            Serial.print(tmp);
          }
          #endif

          free(buf);
          return true;
        } else {
          #if DEBUG
          Serial.println(F("Error in config file."));
          Serial.println(error.c_str());
          #endif
        }
      }

    }
  }
  else
  {
/*
    //clean FS, for testing
    if ( SPIFFS.format() ) {
      #if DEBUG
      Serial.println(F("SPIFFS formatted."));
      #endif
    } else {
      #if DEBUG
      Serial.println(F("Failed to format SPIFFS."));
      #endif
    }
*/
  }
  //end read
  return false;
}
