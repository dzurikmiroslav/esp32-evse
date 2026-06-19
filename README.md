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
 - [Modbus](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/Modbus/) (RS485, TCP)
 - [Lua scripting](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/Lua/)
 - [Nextion HMI](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/Nextion/)
 - [AT commands](https://dzurikmiroslav.github.io/esp32-evse-docs/20-software/AT-Commands/)
 - Scheduler

### Web installer

Easy initial installation of esp32-evse firmware can be performed using your browser.

[Web installer](https://dzurikmiroslav.github.io/esp32-evse/web-installer)

### Device definition method

One firmware to rule them all. Not really :-) one per device platform (ESP32, ESP32-S2...).

There is no need to compile the firmware for your EVSE design. Source code ist not hardcoded to GPIOs or other hardware design features. All code is written in ESP-IDF without additional wrapping layer like Arduino.

All configuration is specified separately form the firmware binary in a configuration file named board.yaml stored on a dedicated partition.
For example, on following scheme is minimal EVSE circuit with ESP32 devkit.

![Minimal circuit](https://dzurikmiroslav.github.io/esp32-evse-docs/images/minimal-circuit.png)

For this circuit there is config file _board.yaml_, for more information's see [YAML schema](board-config/board-config-schema-1.json).

```yaml
deviceName: ESP32 minimal EVSE

button:
  gpio: 0

pilot:
  gpio: 33
  adcChannel: 7
  levels: [2410, 2104, 1797, 1491, 265]

acRelay:
  gpios: [32]
```

### Web interface

Fully responsive web interface is accessible local network IP address on port 80.

Dashboard page

![Dashboard](https://dzurikmiroslav.github.io/esp32-evse-docs/images/web-dashboard.png) 

Settings page

![Settings](https://dzurikmiroslav.github.io/esp32-evse-docs/images/web-settings.png)

Mobile dashboard page

![Dashboard mobile](https://dzurikmiroslav.github.io/esp32-evse-docs/images/web-dashboard-mobile.png)

## Hardware

### ESP32DevkitC EVSE

Dev board with basic functionality, single phase energy meter, RS485. One side pcb, for DIY makers easy to make at home conditions ;-)

[EasyEDA project](https://oshwlab.com/dzurik.miroslav/esp32-devkit-evse)

![ESP32DevkitC](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/esp32devkitc.jpg)

### ESP32-S2 EVSE DIY ALPHA

ESP32-S2 based EVSE with advanced functionality, three phase energy meter, RS485, UART, 1WIRE, RCM, socket lock.

[EasyEDA project](https://oshwlab.com/dzurik.miroslav/esp32s2-diy-evse)

[Wiki page](https://github.com/dzurikmiroslav/esp32-evse/wiki/ESP32%E2%80%90S2-EVSE-DIY-ALPHA)

![ESP32-S2-DA](https://github.com/dzurikmiroslav/esp32-evse/wiki/images/esp32s2da.jpg)

Quick demonstration video

[![Quick demonstration video](https://img.youtube.com/vi/r6YkWEet1aA/hqdefault.jpg)](https://www.youtube.com/shorts/r6YkWEet1aA)

## Donations

ESP32 EVSE firmware is free, but costs money to develop harware and time to develop software.
This gift to the developer would demonstrate your appreciation of this software & hardware and help its future development.

[![GitHub Sponsors](https://img.shields.io/badge/donate-GitHub_Sponsors-blue)](https://github.com/sponsors/dzurikmiroslav)
