# Board config

Board config file is named _board.cfg_ is stored on _cfg_ partition. 
During flashing there is option _BOARD_CONFIG_DEPLOY_ for deploy config and option _BOARD_CONFIG_ say which config will be deployed.
This is required only first time, or after changing configuratrion.
All board configs are stored in [_cfg_](https://github.com/dzurikmiroslav/esp32-evse/tree/doc/cfg) directory.

![Board config](/images/board-config-deploy.png "Board config")

## File format

| Key | Description | Representation |
| --- | --- | --- |
| DEVICE_NAME | Name of the device | char[32] |
| MAX_CHARGING_CURRENT | Maximum charging current | uint8 |
| LED_CHARGING | Has charging led | bool |
| LED_CHARGING_GPIO | Charging led gpio | uint32 | 
| LED_ERROR | Has error led | bool |
| LED_ERROR_GPIO | Error led gpio | uint32 |
| LED_WIFI | Has WiFi connection led | bool |
| LED_WIFI_GPIO | WiFi connection led gpio | uint32 |
| BUTTION_WIFI_GPIO | WiFi button gpio | uint32 |
| PILOT_PWM_GPIO | CP pwm generator gpio | uint32 |
| PILOT_ADC_CHANNEL | CP sensing adc channel | uint32 |
| PILOT_DOWN_TRESHOLD_12 | A state high voltage down treshold in mV | uint16 |
| PILOT_DOWN_TRESHOLD_9 | B state high voltage down treshold in mV | uint16 |
| PILOT_DOWN_TRESHOLD_6 | C state high voltage down treshold in mV | uint16 |
| PILOT_DOWN_TRESHOLD_3 | D state high voltage down treshold in mV | uint16 |
| PILOT_DOWN_TRESHOLD_N12 | B,C,D state low voltage treshold in mV | uint16 |
| PROXIMITY | Has PP detection | bool |
| PROXIMITY_ADC_CHANNEL | PP sensing adc channel | uint32 |
| PROXIMITY_DOWN_TRESHOLD_13 | Cable 13A down treshold | uint16 |
| PROXIMITY_DOWN_TRESHOLD_20 | Cable 20A down treshold | uint16 |
| PROXIMITY_DOWN_TRESHOLD_32 | Cable 32A down treshold | uint16 |
| AC_RELAY_GPIO | AC relay gpio for controlling mains contactor | uint32 |
| SOCKET_LOCK | Has socket lock | bool |
| SOCKET_LOCK_A_GPIO | | uint32 |
| SOCKET_LOCK_B_GPIO | | uint32 |
| SOCKET_LOCK_DETECTION_GPIO | | uint32 |
| SOCKET_LOCK_DETECTION_DELAY | | uint16 |
| SOCKET_LOCK_MIN_BREAK_TIME | | uint16 |
| RCM | Has residual current monitor | bool |
| RCM_GPIO | Residual current monitor gpio | uint32 |
| RCM_TEST_GPIO | Resudal current monitor test gpio | uint32 |
| ENERGY_METER | Energy meter | enum(none, cur, cur_vlt) |
| ENERGY_METER_THREE_PHASES | | bool |
| ENERGY_METER_L1_CUR_ADC_CHANNEL | | uint32 |
| ENERGY_METER_L2_CUR_ADC_CHANNEL | | uint32 |
| ENERGY_METER_L3_CUR_ADC_CHANNEL | | uint32 |
| ENERGY_METER_CUR_SCALE | | float |
| ENERGY_METER_L1_VLT_ADC_CHANNEL | | uint32 |
| ENERGY_METER_L2_VLT_ADC_CHANNEL | | uint32 |
| ENERGY_METER_L3_VLT_ADC_CHANNEL | | uint32 |
| ENERGY_METER_VLT_SCALE | | float |
| AUX_1 | | bool |
| AUX_1_GPIO | | bool |
| AUX_2 | | bool |
| AUX_2_GPIO | | bool |
| AUX_3 | | bool |
| AUX_3_GPIO | | bool |
| TEMP_SENSOR | Temperature sensor | bool |
| TEMP_SENSOR_GPIO | Temperature sensor gpio | uint32 |
| SERIAL_1 | Type of serial | enum(none, uart, rs485) |
| SERIAL_1_NAME | Name of serial | chart[16] |
| SERIAL_1_RXD_GPIO | Serial rx gpio | uint32 |
| SERIAL_1_TXD_GPIO | Serial tx gpio | uint32 |
| SERIAL_1_RTS_GPIO | Serial rts gpio | uint32 |
| SERIAL_2 | Type of serial | enum(none, uart, rs485) |
| SERIAL_2_NAME | Name of serial | chart[16] |
| SERIAL_2_RXD_GPIO | Serial rx gpio | uint32 |
| SERIAL_2_TXD_GPIO | Serial tx gpio | uint32 |
| SERIAL_2_RTS_GPIO | Serial rts gpio | uint32 |
| SERIAL_3 * | Type of serial | enum(none, uart, rs485) |
| SERIAL_3_NAME * | Name of serial | chart[16] |
| SERIAL_3_RXD_GPIO * | Serial rx gpio | uint32 |
| SERIAL_3_TXD_GPIO * | Serial tx gpio | uint32 |
| SERIAL_3_RTS_GPIO * | Serial rts gpio | uint32 |

**Note** Serial 3 is available only on MCU that have more than two uarts.

### PILOT_DOWN_TRESHOLD_X calculation

This table descibe detection of pilot state by measured voltage.

| Level | Nominal voltage | Voltage range |
| --- | --- | --- |
| A high voltage | 12 | 10.5 < ? |
| B high voltage | 9 | 7.5 < 10.5 |
| C high voltage | 6 | 4.5 < 7.5 |
| D high voltage | 3 | 1.5 < 4.5 |
| B,C,D low voltage * | -12 | ? < -10.5 |

\* this for detection diode short or other pilot failure.

This range is implemented by sequence of values PILOT_DOWN_TRESHOLD_X (values are given in mV).

On the following scheme is voltage divider with shift (R2, R4, R6).
Wire `CP_OUT` is connected to the EV, `CP_SENS` is connected to ESP32 adc.

![CP sensing circuit](/images/cp-sens-circuit.png "CP sensing circuit")

The following formula describes the calculation of the `CP_SENS` voltage from the `CP_OUT` voltage.

$$ U_{CP\textunderscore SENS} = \frac{U_{CP \textunderscore OUT} + 3.3 \times \frac{R4}{R6}}{1 + \frac{R4}{R6} + \frac{R4}{R2}} $$

Next, with this formula, I calculate voltage range.

| CP_OUT | CP_SENS |
| --- | --- |
| 10.5 | 2.410426947 |
| 7.5 | 2.104054924 |
| 4.5 | 1.797682901 |
| 1.5 | 1.491310877 |
| -10.5 | 0.2658227848 |

After translate values to mV, the file `board.cfg` will look like this:
```bash
PILOT_DOWN_TRESHOLD_12=2410
PILOT_DOWN_TRESHOLD_9=2104
PILOT_DOWN_TRESHOLD_6=1797
PILOT_DOWN_TRESHOLD_3=1491
PILOT_DOWN_TRESHOLD_N12=265
```

**Note 1**
When designing a new voltage divider, remember the ESP32 adc [_suggested range_](https://docs.espressif.com/projects/esp-idf/en/v4.4.3/esp32/api-reference/peripherals/adc.html#_CPPv425adc1_config_channel_atten14adc1_channel_t11adc_atten_t).
(adc is configured for attenuation 11dB)

**Note 2**
On my EV (VW eUp) in state B,C,D sometimes low voltage is not below 10.5V but smaller. That cause error of diode short. I need to investigate the right value...
