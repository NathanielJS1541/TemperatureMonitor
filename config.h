// --------------------------------------- Delay Constants ----------------------------------------

// The delay when idling waiting for the next reading time.
const int idleDelay = 100;

// The delay between connection attempts to WiFi and the graphite server.
const int connectDelay = 100;

// ------------------------------------------------------------------------------------------------

// ---------------------------------------- LED Constants -----------------------------------------

// RGB LED configuration for the M5Stamp-Pico.
#define LED_TYPE SK6812
#define COLOUR_ORDER GRB
#define NUM_LEDS 1
#define DATA_PIN 27
#define LED_BRIGHTNESS 20
CRGB leds[NUM_LEDS];

// ------------------------------------------------------------------------------------------------

// ----------------------------------- Graphite Server Settings -----------------------------------

// Graphite server to report the readings to.
WiFiClient graphiteClient;
const char* graphiteServer = "192.168.1.12";
const int graphitePort = 2003;
// Start and Max timeout when retrying connections to the graphite server.
// These values are used to generate a random delay to mitigate collisions
// between different sensors on the same network, similar to Ethernet's random back-off.
const int startTimeout = 1;
const int maxTimeout = 500;

// ------------------------------------------------------------------------------------------------

// ----------------------------------- Sensor Reading Settings ------------------------------------

// Graphite String definitions that readings will be reported under. Change these to whatever you'd
// like. The "." ensures that the temperature and humidity readings for this sensor are grouped
// together if you have multiple sensors.
const String tempString = "office.temperature";
const String humidityString = "office.relative_humidity";
//const String tempString = "living_room.temperature";
//const String humidityString = "living_room.relative_humidity";
//const String tempString = "garage.temperature";
//const String humidityString = "garage.relative_humidity";

// Refresh period for sensor readings in seconds. This dictates how often readings get reported to
// the graphite server.
const unsigned int refreshPeriod = 60;

// The time period in seconds which the moving average should be calculated with.
const unsigned int movingAveragePeriod = 5;
// How many samples will be stored in the moving average.
const unsigned int movingAverageSize = refreshPeriod / movingAveragePeriod;

// ------------------------------------------------------------------------------------------------