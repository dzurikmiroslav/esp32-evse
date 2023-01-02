# Modbus

Controller support Modbus slave over TCP/IP and RTU/serial.

Modbus settings are in web interface, when you can enable/disable Modbus TCP server and set Unit ID.
Modbus TCP are listening on port 502.

![Modbus settings](/images/web-settings-modbus.png "Modbus settings")

Modbus RTU can be set in serial settings, any serial interface (UART, RS-485) can be operating in Modbus slave mode. Only one interface can operate in Modbus mode!

![Serial settings](/images/web-settings-serial.png "Serial settings")

Table of Modbus registers. (Note, Register Address starting at zero, Register Number = Register Address + 1)

Register Address | Number Of Registers |  Access | Description | Representation
--- | --- | --- | --- | ---
100 | 1 | R | EVSE state (A=0, B=1, C=2, D=3, E=4, F=5) | uint16
101 | 1 | R | EVSE error number (NONE=0, PILOT_FAULT=1, DIODE_SHORT=2, LOCK_FAULT=3, UNLOCK_FAULT=4, RCM_TRIGGERED=5, RCM_SELFTEST_FAULT=6) | uint16
102 | 1 | R/W | Charging enabled (enabled=1, disabled=0) | uint16
103 | 1 | R | Pending auhorization before start charging, when authorization is required (1 when pending otherwise 0) | uint16
104 | 1 | R/W | Charging current in A*10 | uint16
105 | 2 | R/W | Session consumption limit in Ws | uint32
107 | 2 | R/W | Session elapsed time limit in s | uint32
109 | 1 | R/W | Session underpower limit in W | uint16
110 | 1 | W | Authorize to start charging when is pending (value 1 must be written) | uint16
200 | 1 | R | Charging power in W | uint16
201 | 2 | R | Session elapsed time in s | uint32
203 | 2 | R | Session consumption in Ws | uint32
205 | 2 | R | L1 voltage in mV | uint32
207 | 2 | R | L2 voltage in mV | uint32
209 | 2 | R | L3 voltage in mV | uint32
211 | 2 | R | L1 current in mA | uint32
213 | 2 | R | L2 current in mA | uint32
215 | 2 | R | L3 current in mA | uint32
300 | 1 | R/W | Socket outlet (enabled=1, disabled=0) | uint16
301 | 1 | R/W | RCM (enabled=1, disabled=0) | uint16
302 | 1 | R/W | Require authorization to start charging (enabled=1, disabled=0) | uint16
303 | 1 | R/W | Default charging current in A*10, stored in NVS | uint16
304 | 2 | R/W | Default session consumption limit in Ws, stored in NVS | uint32
306 | 2 | R/W | Default session elapsed time limit in s, stored in NVS | uint32
308 | 1 | R/W | Default session underpower limit in W, stored in NVS | uint16
309 | 1 | R/W | Socket lock operating time in ms | uint16
310 | 1 | R/W | Socket lock break time in ms | uint16
311 | 1 | R/W | Socket lock detection (unlock_high=0, locked_high=1) | uint16
312 | 1 | R/W | Socket lock retry count | uint16
313 | 1 | R/W | Energy meter mode (NONE=0, CUR=1, CUR_VLT=2, PULSE=3) | uint16
314 | 1 | R/W | Energy meter voltage in V, when is not measured | uint16
315 | 1 | R/W | Energy meter pulse amount in Wh | uint16
400 | 2 | R | Uptime in s | uint32
402 | 16 | R | App version | char[16]
418 | 1 | W | Restart (value 1 must be written) | uint16
