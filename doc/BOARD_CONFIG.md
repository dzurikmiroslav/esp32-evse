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
| BUTTION_WIFI_GPIO | WiFi button gpio | uint32 |
| PILOT_PWM_GPIO | CP pwm generator gpio | uint32 |
| PILOT_ADC_CHANNEL | CP sensing adc channel | uint32 |
| PILOT_DOWN_TRESHOLD_12 | A state high voltage down treshold in mV | uint32 |
| PILOT_DOWN_TRESHOLD_9 | B state high voltage down treshold in mV | uint32 |
| PILOT_DOWN_TRESHOLD_6 | C state high voltage down treshold in mV | uint32 |
| PILOT_DOWN_TRESHOLD_3 | D state high voltage down treshold in mV | uint32 |
| PILOT_DOWN_TRESHOLD_N12 | B,C,D state low voltage treshold in mV | uint32 |
| AC_RELAY_GPIO | AC relay gpio for controlling mains contactor | uint32 |

### PILOT_DOWN_TRESHOLD_X calculation

This table descibe detection of pilot state by measured voltage.

| Level | Nominal voltage | Voltage range |
| --- | --- | --- |
| A high voltage | 12 | 10.5 < ? |
| B high voltage | 9 | 7.5 < 10.5 |
| C high voltage | 6 | 4.5 < 7.5 |
| D high voltage | 3 | 1.5 < 4.5 |
| B,C,D low voltage | -12 | ? < -10.5 |

These range is implemented by sequence of values PILOT_DOWN_TRESHOLD_X (values are given in mV).

On [minimal schema](/images/minimal-circuit.png "Minimal circuit"), there is voltage divider with shift (R2, R4, R6) connected on pilot adc.

$$ v_{out} = \frac{v_{in} + v_{in} +  $$

Pilot tresholds depends on resistor values of voltage divider with shift

![Minimal circuit](/images/minimal-circuit.png "Minimal circuit")

 (on example circuit: R2, R4, R6).

