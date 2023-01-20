![ESP32 EVSE](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/logo-full.svg)

J1772 EVSE firmware for ESP32 based devices.

![Build with ESP-IDF](https://github.com/dzurikmiroslav/esp32-evse/workflows/Build%20with%20ESP-IDF/badge.svg)
[![License](https://img.shields.io/github/license/dzurikmiroslav/esp32-evse.svg)](LICENSE.md)

## Key features
 - Hardware abstraction for device design
 - Responsive web-interface
 - OTA update
 - Integrated energy meter
 - [REST](https://github.com/dzurikmiroslav/esp32-evse/wiki/Rest) API
 - MQTT API
 - [Modbus](https://github.com/dzurikmiroslav/esp32-evse/wiki/Modbus) (RS485, TCP)
 - AT commands

### Device definition method

_One firmware to rull them all._ Not really :-) one per device platform (ESP32, ESP32-S2...).

There is no need to compile the firmware for your EVSE design.
Source code ist not hardcoded to GPIOs or other harware design features.
All code is writen in ESP-IDF without additional mapping layer like Arduino.

All configuration is writen outside firmware in configuration file named _board.cfg_ on dedicated parition.
For example, on following sheme is minimal EVSE circuit with ESP32 devkit.

![Minimal circuit](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/minimal-circuit.png)

For this circuit there is _board.cfg_, for more informations see [Wiki](https://github.com/dzurikmiroslav/esp32-evse/wiki/Board-config).


```bash
#Device name
DEVICE_NAME=ESP32 minimal EVSE
#Button
BUTTION_WIFI_GPIO=0
#Pilot  
PILOT_PWM_GPIO=33
PILOT_ADC_CHANNEL=7
PILOT_DOWN_TRESHOLD_12=2410
PILOT_DOWN_TRESHOLD_9=2104
PILOT_DOWN_TRESHOLD_6=1797
PILOT_DOWN_TRESHOLD_3=1491
PILOT_DOWN_TRESHOLD_N12=265
#AC relay
AC_RELAY_GPIO=32
```

### Web interface

Fully responsive web interface is accesible local network IP address on port 80.

Dashboard page

![Dashboard](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/web-dashboard.png) 

Settings page

![Settings](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/web-settings.png)

Mobile dashboard page

![Dashboard mobile](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/web-dashboard-mobile.png)

### Hardware

#### ESP32DevkitC

Dev board with basic functionality, single phase energy meter, RS485. One side pcb, for DIY makers easy to make at home conditions ;-)

[EasyEDA project](https://oshwlab.com/dzurik.miroslav/esp32-devkit-evse)

![ESP32DevkitC](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/esp32devkitc.jpg)

### ESP32-S2 DIY ALPHA

ESP32-S2 based EVSE with advanced functionality, three phase energy meter, RS485, UART, 1WIRE, RCM, socket lock.

**Currenlty in development not tested!**

[EasyEDA project](https://oshwlab.com/dzurik.miroslav/esp32s2-diy-evse)

![ESP32-S2-DA](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/esp32s2da.png)
