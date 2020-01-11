/*
 ESP8266 boblightd

 Requires ESP8266 board & WS2812B NeoPixels

 (c) blaz@kristan-sp.si / 2020-01-10
*/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>          // multicast DNS
#include <WiFiUdp.h>              // UDP handling
#include <ESP8266HTTPClient.h>    // Ota
#include <ESP8266httpUpdate.h>    // Ota
#include <StreamString.h>         // Webserver, Updater
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ArduinoOTA.h>           // Arduino OTA updates

#include <NeoPixelBus.h>

#include "settings.h"
#include "webpages.h"


WiFiClient espClient;                 // Wifi Client
WiFiServer bob(BOB_PORT);
WiFiClient bobClient;
ESP8266WebServer server(80);          // web server object

#if (USE_WS2812_CTYPE == 1)
  NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> *strip = NULL;
#else  // USE_WS2812_CTYPE
  NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod> *strip = NULL;
#endif  // USE_WS2812_CTYPE

// strings (to reduce IRAM pressure)
static const char _CONFIG[] PROGMEM = "/config.conf";

// 108 LEDs will fit on a 55" TV (30 LED/m), organised clockwise, starting middle bottom
unsigned int numLights = 0;
light_t lights[WS2812_MAX_LEDS];


// flag for saving data from WiFiManager
bool shouldSaveConfig = false;

// private functions
char *ftoa(float,char*,int d=2);
void saveConfigCallback ();
void ws2812_init(unsigned int pin);
void pollBob();


//-----------------------------------------------------------
// main setup
void setup() {
  char str[128];
  char clientId[20];
  
  delay(3000);

  #if DEBUG
  Serial.begin(115200);
  #else
  // Initialize the BUILTIN_LED pin as an output & set initial state LED on
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  #endif

  String WiFiMAC = WiFi.macAddress();
  WiFiMAC.replace(":","");
  WiFiMAC.toCharArray(str, 16);
  // Create client ID from MAC address
  sprintf(clientId, WIFI_HOSTNAME, DEFAULT_NAME, &str[6]);

//----------------------------------------------------------

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  //WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // reset settings (for debugging)
  //wifiManager.resetSettings();
  
  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  //wifiManager.addParameter(&custom_mqtt_server);

  // set minimum quality of signal so it ignores AP's under that quality
  // defaults to 8%
  //wifiManager.setMinimumSignalQuality(10);
  
  // sets timeout until configuration portal gets turned off
  // useful to make it all retry or go to sleep
  // in seconds
  //wifiManager.setTimeout(120);

  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(clientId)) {
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi

  //read updated parameters
  //strcpy(mqtt_server, custom_mqtt_server.getValue());

  //save the custom parameters to FS
  if ( shouldSaveConfig ) {
    #if DEBUG
    Serial.println("Saving WifiManager FS data.");
    #endif
  }
//----------------------------------------------------------

  #if !DEBUG
  // if connected set state LED off
  digitalWrite(BUILTIN_LED, HIGH);
  #endif

  // OTA update setup
  //ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(clientId);
  //ArduinoOTA.setPassword("ota_password");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = F("sketch");
    } else { // U_FS
      type = F("filesystem");
    }
    #if DEBUG
    Serial.print(F("Start updating "));
    Serial.println(type);
    #endif
  });
  ArduinoOTA.onEnd([]() {
    #if DEBUG
    Serial.println(F("\nEnd"));
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #if DEBUG
    Serial.printf_P(PSTR("Progress: %u%%\r"), (progress / (total / 100)));
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #if DEBUG
    Serial.printf_P(PSTR("Error[%u]: "), error);
    if      (error == OTA_AUTH_ERROR)    Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR)   Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR)     Serial.println(F("End Failed"));
    #endif
  });
  ArduinoOTA.begin();

  #if DEBUG
  Serial.println("Starting web & bob servers.");
  #endif

  ws2812_init(WS2812_PIN);
  
  // web server setup
  if (MDNS.begin(clientId)) {
    MDNS.addService("http", "tcp", 80);
    #if DEBUG
    Serial.println(F("MDNS responder started."));
    #endif
  }

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/set/", handleSet);
  server.onNotFound(handleNotFound);
  server.begin();

  // read boblight configuration from FS json
  if ( !loadBobConfig() || numLights == 0 ) {
    fillBobLights(16,9,16,9,10);  // 50 lights == 1 WS2811 string
    saveBobConfig();
  }
  
  bob.begin();
  bob.setNoDelay(true);
}

//-----------------------------------------------------------
// main loop
void loop() {
  char tmp[64];
  
  // handle OTA updates
  ArduinoOTA.handle();

  // handle web server request
  server.handleClient();
  MDNS.update();

  pollBob();

  // wait 1ms until next update
  delay(1);
}


// reverses a string 'str' of length 'len' 
void reverse(char *str, int len) 
{ 
    int i=0, j=len-1, temp; 
    while (i<j) 
    { 
        temp = str[i]; 
        str[i] = str[j]; 
        str[j] = temp; 
        i++; j--; 
    } 
} 

// Converts a given integer x to string str.  d is the number 
// of digits required in output. If d is more than the number 
// of digits in x, then 0s are added at the beginning. 
int intToStr(int x, char *str, int d) 
{ 
    int i = 0, s = x<0;
    while (x) 
    { 
      str[i++] = (abs(x)%10) + '0'; 
      x = x/10; 
    } 
  
    // If number of digits required is more, then 
    // add 0s at the beginning 
    while (i < d)
      str[i++] = '0';

    if ( s )
      str[i++] = '-';
  
    reverse(str, i); 
    str[i] = '\0'; 
    return i; 
} 
  
// Converts a floating point number to string. 
char *ftoa(float n, char *res, int afterpoint) 
{ 
  // Extract integer part 
  int ipart = (int)n; 
  
  // Extract floating part 
  float fpart = n - (float)ipart; 
  
  // convert integer part to string 
  int i = intToStr(ipart, res, 0); 
  
  // check for display option after point 
  if (afterpoint != 0) 
  { 
    res[i] = '.';  // add dot 

    // Get the value of fraction part upto given no. 
    // of points after dot. The third parameter is needed 
    // to handle cases like 233.007 
    fpart = fpart * pow(10, afterpoint); 

    intToStr(abs((int)fpart), res + i + 1, afterpoint); 
  }
  return res;
} 

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  shouldSaveConfig = true;
}

// main boblight handling
void pollBob() {
  
  //check if there are any new clients
  if (bob.hasClient())
  {
    //find free/disconnected spot
    if (!bobClient || !bobClient.connected())
    {
      if(bobClient) bobClient.stop();
      bobClient = bob.available();
    }
    //no free/disconnected spot so reject
    WiFiClient bobClient = bob.available();
    bobClient.stop();
  }
  
  //check clients for data
  if (bobClient && bobClient.connected())
  {
    if (bobClient.available())
    {
      //get data from the client
      while (bobClient.available())
      {
        String input = bobClient.readStringUntil('\n');
        //Serial.print("Client: "); Serial.println(input);
        if (input.startsWith("hello"))
        {
          #if DEBUG
          Serial.println(F("hello"));
          #endif
          bobClient.print("hello\n");
        }
        else if (input.startsWith("ping"))
        {
          #if DEBUG
          Serial.println(F("ping 1"));
          #endif
          bobClient.print("ping 1\n");
        }
        else if (input.startsWith("get version"))
        {
          #if DEBUG
          Serial.println(F("version 5"));
          #endif
          bobClient.print("version 5\n");
        }
        else if (input.startsWith("get lights"))
        {
          char tmp[64];
          String answer = "";
          sprintf(tmp, "lights %d\n", numLights);
          #if DEBUG
          Serial.println(tmp);
          #endif
          answer.concat(tmp);
          for (int i=0; i<numLights; i++)
          {
            sprintf(tmp, "light %s scan %2.1f %2.1f %2.1f %2.1f\n", lights[i].lightname, lights[i].vscan[0], lights[i].vscan[1], lights[i].hscan[0], lights[i].hscan[1]);
            #if DEBUG
            Serial.print(tmp);
            #endif
            answer.concat(tmp);
          }
          bobClient.print(answer);
        }
        else if (input.startsWith("set priority"))
        {
          #if DEBUG
          Serial.println(F("set priority not implemented"));
          #endif
          // not implemented
        }
        else if (input.startsWith("set light "))
        { // <id> <cmd in rgb, speed, interpolation> <value> ...
          input.remove(0,10);
          String tmp = input.substring(0,input.indexOf(' '));
          
          int light_id = -1;
          for ( uint16_t i=0; i<numLights; i++ )
          {
            if ( strncmp(lights[i].lightname, tmp.c_str(), 4) == 0 )
            {
              light_id = i;
              break;
            }
          }
          if ( light_id == -1 )
            return;
          
          #if DEBUG
          Serial.print(F("light found "));
          Serial.println(light_id,DEC);
          #endif

          input.remove(0,input.indexOf(' ')+1);
          if ( input.startsWith("rgb ") )
          {
            input.remove(0,4);
            tmp = input.substring(0,input.indexOf(' '));
            uint8_t red = (uint8_t)(255.0f*tmp.toFloat());
            input.remove(0,input.indexOf(' ')+1);        // remove first float value
            tmp = input.substring(0,input.indexOf(' '));
            uint8_t green = (uint8_t)(255.0f*tmp.toFloat());
            input.remove(0,input.indexOf(' ')+1);        // remove second float value
            tmp = input.substring(0,input.indexOf(' '));
            uint8_t blue = (uint8_t)(255.0f*tmp.toFloat());
            
            #if DEBUG
            Serial.println(F("color set "));
            Serial.print(red,HEX);
            Serial.print(green,HEX);
            Serial.print(blue,HEX);
            Serial.println();
            #endif
            
            ws2812_setBobColor(light_id, red, green, blue);
          } // currently no support for interpolation or speed, we just ignore this
        }
        else if (input.startsWith("sync"))
        {
          #if DEBUG
          Serial.println(F("sync"));
          #endif
          ws2812_BobSync();
        }        
        else
        {
          // Client sent gibberish
          bobClient.stop();
          bobClient = bob.available();
          ws2812_clear();
        }
      }
    }
  }
  else
  {
    ws2812_clear();
  }
}

void ws2812_init(unsigned int pin) {
#if (USE_WS2812_CTYPE == 1)
  strip = new NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod>(WS2812_MAX_LEDS, pin);
#else  // USE_WS2812_CTYPE
  strip = new NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang800KbpsMethod>(WS2812_MAX_LEDS, pin);
#endif  // USE_WS2812_CTYPE
  strip->Begin();
  ws2812_clear();
}

void ws2812_clear()
{
  strip->ClearTo(0);
  strip->Show();
}

void ws2812_setBobColor(uint16_t led, uint8_t red, uint8_t green, uint8_t blue)
{
  RgbColor lcolor;
  lcolor.R = red;
  lcolor.G = green;
  lcolor.B = blue;
  
  strip->SetPixelColor(led, lcolor);
}

void ws2812_BobSync()
{
  strip->Show();
}
