![ESP32 EVSE](https://dzurikmiroslav.github.io/esp32-evse-docs/assets/logo-full.svg)

J1772 EVSE firmware for ESP32 based devices.

![Build with ESP-IDF](https://github.com/dzurikmiroslav/esp32-evse/workflows/Build/badge.svg)
[![GitHub version](https://img.shields.io/github/release/dzurikmiroslav/esp32-evse.svg)](https://github.com/dzurikmiroslav/esp32-evse/releases/latest)
[![License](https://img.shields.io/github/license/dzurikmiroslav/esp32-evse.svg)](LICENSE.md)
[![GitHub Sponsors](https://img.shields.io/badge/donate-GitHub_Sponsors-blue)](https://github.com/sponsors/dzurikmiroslav)
[![Web installer](https://img.shields.io/badge/web-installer-green?style=flat&logo=googlechrome&logoColor=lightgrey)](https://dzurikmiroslav.github.io/esp32-evse/web-installer)

## Key features
 - Hardware abstraction for device design
 - Responsive web-interface
 - OTA update
 - Integrated energy meter
 - REST API
 - WebDAV
 - [Modbus](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/Modbus/) (RTU, TCP)
 - [Lua scripting](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/Lua/)
 - [Nextion HMI](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/Nextion/)
 - [AT commands](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/AT-Commands/)
 - Scheduler

### Device definition method

One firmware to rule them all. Not really :-) one per device platform (ESP32, ESP32-S2...).

There is no need to compile the firmware for your EVSE design. Source code ist not hardcoded to GPIOs or other hardware design features. All code is written in ESP-IDF without additional wrapping layer like Arduino.

All configuration is specified separately form the firmware binary in a configuration file named board.yaml stored on a dedicated partition.

[Check out the documentation](https://dzurikmiroslav.github.io/esp32-evse-docs/) for a full list of features and hardware examples.

### Web interface

Fully responsive web interface is accessible local network IP address on port 80.

Dashboard page

![Dashboard](https://dzurikmiroslav.github.io/esp32-evse-docs/images/web-dashboard.png) 

Settings page

![Settings](https://dzurikmiroslav.github.io/esp32-evse-docs/images/web-settings.png)

Mobile dashboard page

![Dashboard mobile](https://dzurikmiroslav.github.io/esp32-evse-docs/images/web-dashboard-mobile.png)

## Donations

ESP32 EVSE firmware is free, but costs money to develop harware and time to develop software.
This gift to the developer would demonstrate your appreciation of this software & hardware and help its future development.

[![GitHub Sponsors](https://img.shields.io/badge/donate-GitHub_Sponsors-blue)](https://github.com/sponsors/dzurikmiroslav)
