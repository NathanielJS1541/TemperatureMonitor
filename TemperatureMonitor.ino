/**************************************************************************************************
This code reads the temperature and humidity from a SHTC3 sensor over I2C, and sends it to a 
Graphite database with a specified time period. This is designed to be robust enough to 
"Set it and forget it" and deals with various issues such as the WiFi network going down, or the
Graphite server handling requests from other sensors.

This is designed to use a SHTC3 Temperature and Humidity sensor connected to an M5Stamp Pico, but 
could probably be very easily adapted to other sensors and microcontrollers.

Hookup Diagram:

| M5Stamp Pico | SHTC3 |
|:------------:|:-----:|
|      3V3 *1  |  Vin  |
|      21      |  SDA  |
|      22      |  SCL  |
|      GND     |  GND  |

*1 - Strictly I believe that the M5Stamp Pico's IO pins are 5V tolerant from looking at the 
     schematic provided by the manufacturer. If you want to trust that then you can go ahead and
     hook the SHTC3 to 5V from the Stamp Pico.

I then powered the M5Stamp Pico through one of the 5V lines using a USB charger and an Adafruit
USB-C breakout.

This sketch will look for a "wifi_config.h" file for the WiFi SSID and Password. You will find a
"wifi_config_example.h" in this repo, simply copy it or rename it to "wifi_config.h" and enter
you WiFi SSID and Password. This makes it easier for me to not accidentally release my WiFi
credentials, and means you don't have to re-enter them every time you "git pull".
**************************************************************************************************/

// --------------------------------------- Library Includes ---------------------------------------

// Library to connect to a WiFi network
#include <WiFi.h>
// Library to make UDP requests
#include <WiFiUdp.h>
// Library to connect to an NTP server and sync the time
#include <NTPClient.h>
// Library for I2C communication
#include <Wire.h>
// Library to communicate with the SHTC3 sensor over I2C
#include <Adafruit_SHTC3.h>
// Library to create a moving average of the sensor readings
#include <MovingAveragePlus.h>
// Library to control the RGB LED on the M5Stamp-Pico
#include <FastLED.h>
// Library for working with strings
#include <string.h>
// Library for working with linked lists
#include <LinkedList.h>

// ------------------------------------------------------------------------------------------------

/**************************************************************************************************
These configuration files can be used to tune a majority of the functionality of this sketch. If 
you want to just get a sensor up and running, start here.
**************************************************************************************************/

// User configuration file
#include "config.h"

// WiFi credentials file - if you're starting out copy or rename "wifi_config_example.h" to
// "wifi_config.h" and fill in the details.
#include "wifi_config.h"

// --------------------------------------- NTP Server Setup ---------------------------------------

// UDP class instance for making UDP requests for the NTP server.
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be changed later with
// setTimeOffset() ). Additionally you can specify the update interval (in milliseconds, can be 
// changed using setUpdateInterval() ).
// This will only send requests to an NTP server with the specified time interval, here I've used
// 600,000 milliseconds = 10 minutes. Whenever you call timeClient.update(), it checks whether an
// update is due, and if it is sends a request. Otherwise it just keeps track of time using the
// internal clock. I've increased from the default to 10 minutes to reduce the load on public ntp
// servers, granted it's not a lot but it's just unnecessary to update every minute.
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 600000);

// ------------------------------------------------------------------------------------------------

// ----------------------------- SHTC3 Sensor and Data Storage Setup ------------------------------

// Temperature and Humidity Sensor instance used to access readings from the SHTC3.
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();
// Variables to store the current sensor data.
sensors_event_t humidity, temp;

// Moving average for the sensor readings to try and reduce some noise I was seeing.
MovingAveragePlus<float> temperatureAverage(movingAverageSize);
MovingAveragePlus<float> humidityAverage(movingAverageSize);

// Structure to store sensor readings with a timestamp into a linked list.
struct SensorReading {
  float temperature;
  float relative_humidity;
  unsigned long timestamp;
};

// If back-off method is unsuccessful, LinkedLists can be used to hold readings if the
// exponential random back-off method fails and the graphite database can't be reached. The 
// readings are stored in this Linked List until the next successful connection, and then they are 
// all sent at once and removed from the list.
LinkedList<SensorReading> UnprocessedReadings = LinkedList<SensorReading>();

// ------------------------------------------------------------------------------------------------

// ------------------------------- Global Update Tracking Variables -------------------------------

// The Epoch time of the last sensor update.
unsigned long lastUpdateTime = 0;
// The epoch time to take the next sensor reading.
unsigned long nextUpdateTime = 0;
// The next time a sample should be added to the moving average.
unsigned long nextAverageTime = 0;

// ------------------------------------------------------------------------------------------------

// --------------------------------------- Helper Functions ---------------------------------------

// This function will check whether the WiFi connection is good. This function is blocking until
// a connection is established.
void CheckWiFiConnection(){
  // If WiFi is not connected. This will set the LED to blue until the WiFi reconnects.
  while (!WiFi.isConnected()) {
    // Change LED colour to signal WiFi issue.
    leds[0] = CRGB::Blue;
    FastLED.show(LED_BRIGHTNESS);

    // Attempt to reconnect to the WiFi network.
    WiFi.reconnect();

    // Delay repeated connection attempts to not flood the AP.
    delay(connectDelay);
  }

  // Turn the LED off to signal that the WiFi has connected again.
  leds[0] = CRGB::Black;
  FastLED.show(LED_BRIGHTNESS);
}

// Function to return the string used to submit the reading to the graphite database. This just
// helps to de-clutter the code a bit.
String GetReadingString(String sensorString, float reading, unsigned long timestamp)
{
  return sensorString + " " + String(reading) + " " + String(timestamp);
}

// Function to send readings to the graphite server. 
void SubmitReadings(float temperature, float humidity, unsigned long timestamp, 
                    WiFiClient& graphiteClient)
{
  // Ensure that there is a WiFi connection and stop here until there is one.
  CheckWiFiConnection();

  // Get the strings that contain the reading name, sensor values and timestamp.
  String temperaturePacket = GetReadingString(tempString, temperature, timestamp);
  String humidityPacket = GetReadingString(humidityString, humidity, timestamp);

  // Send the data to the graphite server.
  Serial.println("Sending Data Packets:");
  Serial.println(temperaturePacket);
  graphiteClient.println(temperaturePacket);
  graphiteClient.println();
  Serial.println(humidityPacket);
  graphiteClient.println(humidityPacket);
  graphiteClient.println();
}

// ------------------------------------------------------------------------------------------------

void setup()
{
  // Set up i2c serial interface on the default pins 8 (SDA) and 9 (SCL).
  Wire.begin();

  // Set up serial debug interface at 115200 baud.
  Serial.begin(115200);

  // Initialise the on-board LED.
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOUR_ORDER>(leds, NUM_LEDS);

  // Set LED colour to blue to indicate connecting to WiFi.
  leds[0] = CRGB::Blue;
  FastLED.show(LED_BRIGHTNESS);

  // Connect to the WiFi network specified in wifi_config.h
  WiFi.begin(ssid, password);
  Serial.print("Wifi Connecting.");
  while(WiFi.status() != WL_CONNECTED) {
    delay(connectDelay);
    Serial.print(".");
  }
  Serial.println(" WiFi Connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Once connected to WiFi, set LED colour to yellow to indicate waiting for I2C connection.
  leds[0] = CRGB::Yellow;
  FastLED.show(LED_BRIGHTNESS);

  // Initialise the SHTC3 using the Adafruit Library. The SHTC3 seems to have a bit of an
  // unreliable I2C interface which sometimes requires you to power off then on the snesor before
  // being able to connect to it. There is a note about this on Adafruit's website with no fix yet.
  // If the LED is stuck on yellow, just power everything off and try again after a few seconds.
  Serial.print("Initialising SHTC3.");
  while(!shtc3.begin()) {
    Serial.print(".");
  }
  Serial.println(" SHTC3 Initialised.");

  // Once SHTC3 is connected, change LED colour to red to indicate waiting for NTP sync.
  leds[0] = CRGB::Red;
  FastLED.show(LED_BRIGHTNESS);

  // Start NTP client.
  Serial.print("Initialising NTP Client.");
  timeClient.begin();
  while(!timeClient.update()) {
    Serial.print(".");
  }
  Serial.println(" Connected to NTP Server.");

  // Initialise the moving average array quickly with data from the sensor.
  for(unsigned int reading = 0; reading < movingAverageSize; reading++) {
    shtc3.getEvent(&humidity, &temp);
    temperatureAverage.push(temp.temperature);
    humidityAverage.push(humidity.relative_humidity);
  }
  // Set timer variables at the time when the moving average arrays were initialised.
  lastUpdateTime = timeClient.getEpochTime();
  nextUpdateTime = lastUpdateTime + refreshPeriod;
  nextAverageTime = lastUpdateTime + movingAveragePeriod;

  // If everything successfully initialised, turn the LEDs off to avoid wear.
  leds[0] = CRGB::Black;
  FastLED.show(LED_BRIGHTNESS);
}

void loop()
{
  // Until it is time to take the next reading, just average samples using the moving averages.
  while(timeClient.getEpochTime() < nextUpdateTime) {
    // Check whether samples should be added to the moving average.
    if(timeClient.getEpochTime() > nextAverageTime) {
      // Update the current time if required.
      timeClient.update();

      // Get temperature and humidity data at the current time.
      shtc3.getEvent(&humidity, &temp);

      // Update the last update time to the current time.
      lastUpdateTime = timeClient.getEpochTime();

      // Add the temperature and humidity results to the moving average.
      temperatureAverage.push(temp.temperature);
      humidityAverage.push(humidity.relative_humidity);

      // Update the time to take the next average.
      nextAverageTime += movingAveragePeriod;
    }
  }

  // Ensure that there is a WiFi connection and stop here until there is one.
  CheckWiFiConnection();

  // Setup variables for the timeout handler.
  int retries = 0;
  int currentMaxTimeout = 0;
  bool dataSubmitted = false;

  // Attempt to connect to the graphite server with an exponential back-off.
  Serial.print("Connecting to Graphite");
  while ((dataSubmitted == false) && (currentMaxTimeout < maxTimeout)) {
    Serial.print(".");

    if (!graphiteClient.connect(graphiteServer, graphitePort)) {
      // If connecting to the server was unsucessful, calculate the maximum delay for this retry.
      currentMaxTimeout = pow(2, retries);

      // Delay for a random amount between 0 and currentMaxTimeout.
      delay(random(currentMaxTimeout));
    }
    else {
      Serial.println(" Graphite connected.");

      // Send all data on a successful connection, including any backlog in the LinkedList.
      while ((UnprocessedReadings.size() != 0) && WiFi.isConnected()) {
        // While there are unprocessed readings, get (and remove) the first item to process.
        SensorReading readingToProcess = UnprocessedReadings.shift();

        SubmitReadings(readingToProcess.temperature, readingToProcess.relative_humidity,
                       readingToProcess.timestamp, graphiteClient);
      }

      // Submit current data after backlog is cleared.
      SubmitReadings(temperatureAverage.get(), humidityAverage.get(),
                     lastUpdateTime, graphiteClient);

      // Signal that the readings were submitted so the loop can close.
      dataSubmitted = true;
    }

    // Stop any currently open socket.
    graphiteClient.stop();

    // Increment retries variable
    retries++;
  }

  // If the loop was exited but the data was not submitted, add it to the unprocessed readings LinkedList.
  if (!dataSubmitted) {
    // Add the sensor readings to the data structure.
    SensorReading unprocessedReading = {temperatureAverage.get(), humidityAverage.get(), lastUpdateTime};

    // Add the readings to the end of the linked list.
    UnprocessedReadings.add(unprocessedReading);
  }

  // Set the time when the next update is needed.
  nextUpdateTime += refreshPeriod;
}