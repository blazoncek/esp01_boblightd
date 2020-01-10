## Boblight
Provide ESP8266 based Boblight based on WS2812. This work is derived from  altelch / Bob8266.


### Version Information

- Boblight starts with 50 LEDs default (16-9-16-9) with depth of scan of 10%.
- Supports ESP01 and default GPIO2 for WS2812B LED strip.
- Supports web configuration of LEDs in the form of: # of bottom LED, # of left LEDs, ...
- Stores LED configuration in SPIFFS file.
- Use 64k SPIFFS on ESP01 to support Arduino OTA updates.

### License

This program is licensed under GPL-3.0
