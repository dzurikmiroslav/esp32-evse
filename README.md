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

![The San Juan Mountains are beautiful!](/images/minimal-circuit.png "Minimal circuit")


### Web interface
