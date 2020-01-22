// global includes (libraries)

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <ESP8266mDNS.h>          // multicast DNS
#include <WiFiUdp.h>              // UDP handling
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal

#include "settings.h"
#include "webpages.h"


File fsUploadFile;


void handleRoot() {
  char buffer[128];
  int b=0,r=0,t=0,l=0;
  String postForm = 
"<html>\n"
"<head>\n"
  "<title>Configure LED strips</title>\n"
  "<style>body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>\n"
"</head>\n"
"<body>\n"
"<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/set/\">\n"
"<table>\n";

  for ( int i=0; i<numLights; i++ )
  {
    switch ( lights[i].lightname[0] )
    {
      case 'b' : b++; break;
      case 'l' : l++; break;
      case 't' : t++; break;
      case 'r' : r++; break;
    }
  }
  snprintf_P(buffer, 127, PSTR("<tr><td>Bottom LEDs:</td><td><input type=\"text\" name=\"bottom\" value=\"%d\"></td></tr>"), b);
  postForm += buffer;
  snprintf_P(buffer, 127, PSTR("<tr><td>Left LEDs:</td><td><input type=\"text\" name=\"left\" value=\"%d\"></td></tr>"), l);
  postForm += buffer;
  snprintf_P(buffer, 127, PSTR("<tr><td>Top LEDs:</td><td><input type=\"text\" name=\"top\" value=\"%d\"></td></tr>"), t);
  postForm += buffer;
  snprintf_P(buffer, 127, PSTR("<tr><td>Right LEDs:</td><td><input type=\"text\" name=\"right\" value=\"%d\"></td></tr>"), r);
  postForm += buffer;

  postForm += "<tr><td>Depth of scan (%):</td><td><input type=\"text\" name=\"pct\" value=\"10\"></td></tr>";

  postForm += "<tr><td colspan=\"2\" align=\"center\"><input type=\"submit\" value=\"Submit\"></td></tr>\n</table>\n</form>\n</body>\n</html>";
    
  server.send(200, "text/html", postForm);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void handleSet() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    if (server.args() == 0) {
      return server.send(500, "text/plain", "BAD ARGS");
    }

    int zones;
    char tnum[4];
  
    String message = F(
"<html>\n"
"<head>\n"
  "<meta http-equiv='refresh' content='15; url=/' />\n"
  "<title>ESP8266 settings applied</title>\n"
"</head>\n"
"<body>\n"
  "Settings applied.<br>\n"
      );

    for ( uint8_t i = 0; i < server.args(); i++ ) {
      String argN = server.argName(i);
      String argV = server.arg(i);

      if ( argN != "plain" ) {
        message += " " + argN + ": " + argV + "<br>\n";
      } else {
        continue;
      }
    }

    int bottom = server.arg("bottom").toInt();
    int left   = server.arg("left").toInt();
    int top    = server.arg("top").toInt();
    int right  = server.arg("right").toInt();
    float pct  = server.arg("pct").toFloat();

    if ( bottom+left+top+right > WS2812_MAX_LEDS )
    {
      message += "Too many LEDs specified. Try lower values.<br>\n";
      message += "</body>\n</html>";
      server.send(200, "text/html", message);
    }
    else
    {
      message += "<br>ESP is restarting, please wait.<br>\n";
      message += "</body>\n</html>";
      server.send(200, "text/html", message);

      fillBobLights(bottom, left, top, right, pct);
      saveBobConfig();

      // restart ESP
      ESP.reset();
      delay(2000);
  
    }

  }
}
