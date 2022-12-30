# EVSE firmware for ESP32 based devices

This repository contains J1772 EVSE firmware for ESP32 based devices.

![Build with ESP-IDF](https://github.com/dzurikmiroslav/esp32-evse/workflows/Build%20with%20ESP-IDF/badge.svg)
[![License](https://img.shields.io/github/license/dzurikmiroslav/esp32-evse.svg)](LICENSE.md)

## Key features

 - Generic firmware
 - Responsive web-interface
 - OTA update
 - Integrated energy meter
 - REST API
 - MQTT API
 - Modbus (RS485, TCP)
 - AT commands

### Generic firmware

_One firmware to rull them all._ Not really :-), one per device platform (ESP32, ESP32-S2...).

Firmware ist not hardcoded to GPIOs or oher harware design features. All configuration is writen outside firmware in configuration file on dedicated parition.

For example, on following sheme is minimal EVSE circuit with ESP32. Wihout 

![Minimal circuit](/images/minimal-circuit.png "Minimal circuit")

For this circuit there is configuration file _board.cfg_

```bash
#Device name
DEVICE_NAME=ESP32 minimal EVSE

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
