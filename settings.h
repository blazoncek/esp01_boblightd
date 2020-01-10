#define DEBUG   0

#define DEFAULT_NAME           "bob"    // Default Device Name
#define WIFI_HOSTNAME          "%s-%04d"    // Expands to <NAME>-<last 4 decimal chars of MAC address>

#define WS2812_MAX_LEDS         150         // Max number of LEDs don't go above 150 due to ArduinoJson memory problems (150 pixels are on 5m strip spaced at 30 pixels/m)
#define WS2812_PIN              2           // GPIO2 (for compatibility with ESP01)
#define USE_WS2812_CTYPE        1           // WS2812 Color type (0 - RGB, 1 - GRB)

#define BOB_PORT                19333       // Default boblightd port

typedef struct _LIGHT {
  char lightname[5];
//  uint8_t rgb[3];
  float hscan[2];
  float vscan[2];
} light_t;

extern light_t lights[];
extern unsigned int numLights;


bool loadBobConfig();
bool saveBobConfig();
void fillBobLights(int,int,int,int,float);
