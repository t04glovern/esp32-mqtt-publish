# esp32-mqtt-publish

## Setup

Edit the `src/main.h.example` file with the relevant information for your project and save it as `main.h`

```cpp
#ifndef MAIN_H

// Wifi Details
const char *ssid = "YourWifiSSID";
const char *password = "YourWifiPassword";

// MQTT Details
const char *mqtt_server = "YourMQTTServer";
const char *mqtt_user = "MQTTUser";
const char *mqtt_pass = "MQTTPass";
const char *mqtt_in_topic = "MQTTTopicIn";
const char *mqtt_out_topic = "MQTTTopicOut";
const int mqtt_port = MQTTServerPort;

#endif
```

## MQTT Providers

You can sign up for a free MQTT instance with [CloudMQTT](https://www.cloudmqtt.com/) and select the free `Cat` 

## Platform IO

This project is build and run with PlatformIO. The library dependencies can be found in the `platformio.ini` file. Below is the current configuration targetting the FireBeetle ESP32 development board. This can be changed to any variable of the ESP32 chip.

```ini
[env:firebeetle32]
platform = espressif32
board = firebeetle32
framework = arduino

lib_deps =
    Adafruit MMA8451 Library@1.0.3
    PubSubClient@2.6
```
