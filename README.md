# Temperature Monitor Sketch for M5Stamp Pico and SHTC3 over WiFi

## About This Sketch {#about}
This is an Arduino sketch to allow an M5Stamp Pico to take temperature and relative humidity readings from an I2C SHTC3 sensor, and report them to a Graphite server over a WiFi network. It assumes you already have a Graphite server setup. This sketch is designed to be robust enough to take long-term readings, and allow easy customisation using the `config.h` and `wifi_config.h` header files.

Although the sketch will work "out of the box" with an M5Stamp Pico and SHTC3, it should be trivial to convert it to any WiFi-enabled micro-controller variant and temperature/humidity sensor.

## Getting Started {#getting-started}
In order to get started with this sketch, please ensure you have all the [Prerequisites](#prerequisites). You **must** then read the [Customising the Sketch](#customisation) section, or the sketch **will not compile**.

#### Hookup Diagram:
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

## Prerequisites {#prerequisites}
- If you are using an M5Stamp Pico, you must have the M5Stamp Pico added to your **Additional Board Manager** within the Arduino IDE. The steps   for this can be found [here](https://docs.m5stack.com/en/quick_start/stamp_pico/arduino), but in short:
  - In the Arduino IDE, go to **File** -> **Preferences** -> **Additional Boards Manager URLs**
  - Into that box, paste in `https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json`
  - Ensure that if there are other URLs already there, that they are comma separated
  - Press **Ok**
  - You can now select this board using **Tools** -> **Board** -> **M5Stack** -> **STAMP-PICO**
- If you are using a SHTC3, the **Adafruit SHTC3** library installed. There is a guide [here](https://learn.adafruit.com/adafruit-sensirion-shtc3-temperature-humidity-sensor/arduino) with a test script, but in short:
  - Go to **Sketch** -> **Include Library** -> **Manage Libraries**
  - Search for **Adafruit SHTC3**
  - Click **Install**
  - At this point it may be wise to try and run the test script, to ensure your wiring is good.
- A running [Graphite](https://graphiteapp.org/) server running on your network, and **optionally** a [Grafana](https://grafana.com/) server running locally to pull data from the Graphite server.

## Customising the Sketch {#customisation}
### WiFi Network
This sketch will look for a **wifi_config.h** file for the WiFi SSID and Password. You will find a
**wifi_config_example.h** in this repo, simply copy it or rename it to **wifi_config.h** and enter
you WiFi SSID and Password. This makes it easier for me to not accidentally release my WiFi
credentials, and means you don't have to re-enter them every time you `git pull`.

### Config.h
The **config.h** file contains the constants that define the behaviour of the sensor. Most are self explanatory from the comments, so I will only highlight the most important here.

#### LED Constants
This is set up assuming that you are using an M5Stamp Pico, as it defines a CRGB SK6812 LED on pin 27; this is the on-board LED. If you are using a different micro-controller or want to use an external LED, this is where you would change that. The brightness can also be changed here. These changes are fairly trivial if you want to use an LED that is supported by the [FastLED](https://github.com/FastLED/FastLED) library, but if it's not you will need to make changes in the main sketch.

#### Graphite Server Settings
These settings define how the sensor will try and communicate with a Graphite server. Enter the correct IP address and a port if you changed it from the default.

The timeouts are used for the exponential random back-off that is implemented in case the Graphite server can't answer a request at the moment the sensor sends one. This could be the case if you have multiple sensors or are looking at the data. This is similar to how Ethernet networks avoid packet collision. You can leave these alone unless you are having problems where the sensors aren't being recorded, in which case you can try increasing `maxTimeout`.

#### Sensor Reading Settings
The strings define what name the readings are sent to the Graphite server under. Graphite "groups" readings together where they have matching components, separated by a ".". For Example:
```c
const String tempString = "office.temperature";
const String humidityString = "office.relative_humidity";
```
This will create a group on the Graphite server called **office**, and then create separate readings under that group called **temperature** and **humidity**. Change these to whatever is relevant to your application.

The `refreshPeriod` is how often (in seconds) the sensor will send readings to the Graphite server. By default this is 60 seconds. Increase this if you want less precision and value storage space, decrease it if you want to detect gusts of wind.

The moving average is used to try and filter out some of the noise that my sensor seemed to have. The `movingAveragePeriod` defines the time period (in seconds) that results will be added to the moving average. By default, results are added to the moving average every 5 seconds, which means each reading the Graphite server gets is an average of 12 readings. Note that while this helps to remove noise, it also has a damping effect on the readings. The more results are in this moving average, the more that extreme and sudden changes will be damped. Disabling this would give you more control over the readings in something like Grafana, but will increase the load on you Graphite server, and may dramatically increase the amount of storage you use.

## Troubleshooting {#troubleshooting}
The on-board LED is used to display colours which will indicate a certain issue. These colours are listed here with their meanings.

| Colour |                   Error                  |
|:------:|:----------------------------------------:|
|  Blue  | Can't connect to specified WiFi network. |
| Yellow |     Can't initialise SHTC3 over I2C.     |
|   Red  |         Can't sync to NTP server.        |

These colours will show briefly under normal initialisation, but if they are present over extended periods it will indicate a problem with that stage.

Note that the SHTC3 can sometimes fail to initialise properly and a yellow LED will remain on. Try reconnecting the sensor or power cycling.

## Future Plans {#todo}
- [x] Add more checks for WiFi connectivity before trying to send readings.
- [ ] Decrease power usage by disconnecting WiFi when readings aren't being sent, additionally maybe use a library like [Adafruit's SleepyDog](https://github.com/adafruit/Adafruit_SleepyDog) to put the micro-controller into a lower power state?