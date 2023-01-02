# REST API

> Version 1.0

REST API are listening on port 80.
Default not require authentication.
For reset authentication credentials, hold WiFi button for 10 seconds.
(Note: everything configuration in NVS will be erased!)

## Path Table

| Method | Path | Description |
| --- | --- | --- |
| GET | [/boardConfig](#getboardconfig) | Get board config |
| GET | [/config](#getconfig) | Get full config |
| GET | [/config/evse](#getconfigevse) | Get EVSE config |
| POST | [/config/evse](#postconfigevse) | Set EVSE config |
| GET | [/config/modbus](#getconfigmodbus) | Get modbus config |
| POST | [/config/modbus](#postconfigmodbus) | Set modbus config |
| GET | [/config/mqtt](#getconfigmqtt) | Get MQTT config |
| POST | [/config/mqtt](#postconfigmqtt) | Set MQTT config |
| GET | [/config/serial](#getconfigserial) | Get serial config |
| POST | [/config/serial](#postconfigserial) | Set serial config |
| GET | [/config/tcpLogger](#getconfigtcplogger) | Get TCP logger config |
| POST | [/config/tcpLogger](#postconfigtcplogger) | Set TCP logger config |
| GET | [/config/wifi](#getconfigwifi) | Get WiFi config |
| POST | [/config/wifi](#postconfigwifi) | Set WiFi config |
| POST | [/credentials](#postcredentials) | Set authorization credentials |
| POST | [/firmware/checkUpdate](#postfirmwarecheckupdate) | Check for new firmware version on OTA server |
| POST | [/firmware/update](#postfirmwareupdate) | Update firmware from OTA server |
| POST | [/firmware/upload](#postfirmwareupload) | Upload firmware |
| GET | [/info](#getinfo) | Get info |
| POST | [/restart](#postrestart) | Restart device |
| GET | [/state](#getstate) | Get state |

## Reference Table

| Name | Path | Description |
| --- | --- | --- |
| BoardConfig | [#/components/schemas/BoardConfig](#componentsschemasboardconfig) |  |
| Credentials | [#/components/schemas/Credentials](#componentsschemascredentials) |  |
| EvseConfig | [#/components/schemas/EvseConfig](#componentsschemasevseconfig) |  |
| FirmwareCheck | [#/components/schemas/FirmwareCheck](#componentsschemasfirmwarecheck) |  |
| FullConfig | [#/components/schemas/FullConfig](#componentsschemasfullconfig) |  |
| Info | [#/components/schemas/Info](#componentsschemasinfo) |  |
| ModbusConfig | [#/components/schemas/ModbusConfig](#componentsschemasmodbusconfig) |  |
| MqttConfig | [#/components/schemas/MqttConfig](#componentsschemasmqttconfig) |  |
| SerialConfig | [#/components/schemas/SerialConfig](#componentsschemasserialconfig) |  |
| SerialDataBits | [#/components/schemas/SerialDataBits](#componentsschemasserialdatabits) | Data bits |
| SerialMode | [#/components/schemas/SerialMode](#componentsschemasserialmode) | Serial operating mode |
| SerialParity | [#/components/schemas/SerialParity](#componentsschemasserialparity) | Parity |
| SerialStopBits | [#/components/schemas/SerialStopBits](#componentsschemasserialstopbits) | Stop bits |
| SerialType | [#/components/schemas/SerialType](#componentsschemasserialtype) | Serial type |
| State | [#/components/schemas/State](#componentsschemasstate) |  |
| TcpLoggerConfig | [#/components/schemas/TcpLoggerConfig](#componentsschemastcploggerconfig) |  |
| WifiConfig | [#/components/schemas/WifiConfig](#componentsschemaswificonfig) |  |
| basicAuth | [#/components/securitySchemes/basicAuth](#componentssecurityschemesbasicauth) |  |

## Path Details

***

### [GET]/boardConfig

- Summary  
Get board config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  aux1: boolean
  aux2: boolean
  aux3: boolean
  deviceName: string
  energyMeter: string
  energyMeterThreePhases: boolean
  maxChargingCurrent: integer
  proximity: boolean
  rcm: boolean
  // Serial type
  serial1: string
  serial1Name: string
  serial2:#/components/schemas/SerialType
  serial2Name: string
  serial3:#/components/schemas/SerialType
  serial3Name: string
  socketLock: boolean
  socketLockMinBreakTime: integer
}
```

- 401 Not authenticated

***

### [GET]/config

- Summary  
Get full config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  evse: {
    // Charging current in A
    chargingCurrent?: number
    // Default charging current in A, stored in NVS
    defaultChargingCurrent?: number
    // Require authorization to start charging
    requireAuth?: boolean
    // Socket outlet functionality (proximity pilot sensing and socket lock if available)
    socketOutlet?: boolean
    // Residual current monitor
    rcm?: boolean
    // Session consumption limit in Ws
    consumptionLimit?: integer
    // Default session consumption limit in Ws, stored in NVS
    defaultConsumptionLimit?: integer
    // Session elapsed time limit in s
    elapsedLimit?: integer
    // Default session elapsed time limit in s, stored in NVS
    defaultElapsedLimit?: integer
    // Session underpower limit in W
    underPowerLimit?: integer
    // Default session underpower limit in W, stored in NVS
    defaultUnderPowerLimit?: integer
    // Socket lock operating time in ms
    socketLockOperatingTime?: integer
    // Socket lock break time in ms
    socketLockBreakTime?: integer
    // Socket lock detection (unlock_high=false, locked_high=true)
    socketLockDetectionHigh?: boolean
    // Socket lock retry count
    socketLockRetryCount?: integer
    // Energy meter mode
    energyMeter?: string
    // Energy meter voltage in V, when is not measured
    acVoltage?: integer
    // Energy meter pulse amount in Wh
    pulseAmount?: integer
    aux1?: string
    aux2?: string
    aux3?: string
  }
  modbus: {
    // Enable Modbus TCP/IP slave on port 502
    tcpEnabled: boolean
    // Modbus Unit ID
    unitId: integer
  }
  mqtt: {
    // Enable MQTT
    enabled: boolean
    // Server url
    server: string
    // Base topic
    baseTopic: string
    // User, empty string if not required
    user: string
    // Passport, empty string if not required
    password: string
    // Periodicity publishing state topic in s
    periodicity: integer
  }
  serial: {
    // Serial operating mode
    serial1Mode: string
    // Baud rate
    serial1BaudRate: integer
    // Data bits
    serial1DataBits: string
    // Stop bits
    serial1StopBits: string
    // Parity
    serial1Parity: string
    serial2Mode:#/components/schemas/SerialMode
    // Baud rate
    serial2BaudRate: integer
    serial2DataBits:#/components/schemas/SerialDataBits
    serial2StopBits:#/components/schemas/SerialStopBits
    serial2Parity:#/components/schemas/SerialParity
    serial3Mode:#/components/schemas/SerialMode
    // Baud rate
    serial3BaudRate: integer
    serial3DataBits:#/components/schemas/SerialDataBits
    serial3StopBits:#/components/schemas/SerialStopBits
    serial3Parity:#/components/schemas/SerialParity
  }
  tcpLogger: {
    // Enable TCP logger on port 3000
    enabled: boolean
  }
  wifi: {
    // Enable WiFi STA connection
    enabled: boolean
    // WiFi SSID
    ssid: string
    // Passport, empty string if not required
    password?: string
  }
}
```

- 401 Not authenticated

***

### [GET]/config/evse

- Summary  
Get EVSE config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Charging current in A
  chargingCurrent?: number
  // Default charging current in A, stored in NVS
  defaultChargingCurrent?: number
  // Require authorization to start charging
  requireAuth?: boolean
  // Socket outlet functionality (proximity pilot sensing and socket lock if available)
  socketOutlet?: boolean
  // Residual current monitor
  rcm?: boolean
  // Session consumption limit in Ws
  consumptionLimit?: integer
  // Default session consumption limit in Ws, stored in NVS
  defaultConsumptionLimit?: integer
  // Session elapsed time limit in s
  elapsedLimit?: integer
  // Default session elapsed time limit in s, stored in NVS
  defaultElapsedLimit?: integer
  // Session underpower limit in W
  underPowerLimit?: integer
  // Default session underpower limit in W, stored in NVS
  defaultUnderPowerLimit?: integer
  // Socket lock operating time in ms
  socketLockOperatingTime?: integer
  // Socket lock break time in ms
  socketLockBreakTime?: integer
  // Socket lock detection (unlock_high=false, locked_high=true)
  socketLockDetectionHigh?: boolean
  // Socket lock retry count
  socketLockRetryCount?: integer
  // Energy meter mode
  energyMeter?: string
  // Energy meter voltage in V, when is not measured
  acVoltage?: integer
  // Energy meter pulse amount in Wh
  pulseAmount?: integer
  aux1?: string
  aux2?: string
  aux3?: string
}
```

- 401 Not authenticated

***

### [POST]/config/evse

- Summary  
Set EVSE config

#### RequestBody

- application/json

```ts
{
  // Charging current in A
  chargingCurrent?: number
  // Default charging current in A, stored in NVS
  defaultChargingCurrent?: number
  // Require authorization to start charging
  requireAuth?: boolean
  // Socket outlet functionality (proximity pilot sensing and socket lock if available)
  socketOutlet?: boolean
  // Residual current monitor
  rcm?: boolean
  // Session consumption limit in Ws
  consumptionLimit?: integer
  // Default session consumption limit in Ws, stored in NVS
  defaultConsumptionLimit?: integer
  // Session elapsed time limit in s
  elapsedLimit?: integer
  // Default session elapsed time limit in s, stored in NVS
  defaultElapsedLimit?: integer
  // Session underpower limit in W
  underPowerLimit?: integer
  // Default session underpower limit in W, stored in NVS
  defaultUnderPowerLimit?: integer
  // Socket lock operating time in ms
  socketLockOperatingTime?: integer
  // Socket lock break time in ms
  socketLockBreakTime?: integer
  // Socket lock detection (unlock_high=false, locked_high=true)
  socketLockDetectionHigh?: boolean
  // Socket lock retry count
  socketLockRetryCount?: integer
  // Energy meter mode
  energyMeter?: string
  // Energy meter voltage in V, when is not measured
  acVoltage?: integer
  // Energy meter pulse amount in Wh
  pulseAmount?: integer
  aux1?: string
  aux2?: string
  aux3?: string
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Config updated"
}
```

- 400 Bad request

- 401 Not authenticated

***

### [GET]/config/modbus

- Summary  
Get modbus config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Enable Modbus TCP/IP slave on port 502
  tcpEnabled: boolean
  // Modbus Unit ID
  unitId: integer
}
```

- 401 Not authenticated

***

### [POST]/config/modbus

- Summary  
Set modbus config

#### RequestBody

- application/json

```ts
{
  // Enable Modbus TCP/IP slave on port 502
  tcpEnabled: boolean
  // Modbus Unit ID
  unitId: integer
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Config updated"
}
```

- 400 Bad request

- 401 Not authenticated

***

### [GET]/config/mqtt

- Summary  
Get MQTT config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Enable MQTT
  enabled: boolean
  // Server url
  server: string
  // Base topic
  baseTopic: string
  // User, empty string if not required
  user: string
  // Passport, empty string if not required
  password: string
  // Periodicity publishing state topic in s
  periodicity: integer
}
```

- 401 Not authenticated

***

### [POST]/config/mqtt

- Summary  
Set MQTT config

#### RequestBody

- application/json

```ts
{
  // Enable MQTT
  enabled: boolean
  // Server url
  server: string
  // Base topic
  baseTopic: string
  // User, empty string if not required
  user: string
  // Passport, empty string if not required
  password: string
  // Periodicity publishing state topic in s
  periodicity: integer
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Config updated"
}
```

- 400 Bad request

- 401 Not authenticated

***

### [GET]/config/serial

- Summary  
Get serial config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Serial operating mode
  serial1Mode: string
  // Baud rate
  serial1BaudRate: integer
  // Data bits
  serial1DataBits: string
  // Stop bits
  serial1StopBits: string
  // Parity
  serial1Parity: string
  serial2Mode:#/components/schemas/SerialMode
  // Baud rate
  serial2BaudRate: integer
  serial2DataBits:#/components/schemas/SerialDataBits
  serial2StopBits:#/components/schemas/SerialStopBits
  serial2Parity:#/components/schemas/SerialParity
  serial3Mode:#/components/schemas/SerialMode
  // Baud rate
  serial3BaudRate: integer
  serial3DataBits:#/components/schemas/SerialDataBits
  serial3StopBits:#/components/schemas/SerialStopBits
  serial3Parity:#/components/schemas/SerialParity
}
```

- 401 Not authenticated

***

### [POST]/config/serial

- Summary  
Set serial config

#### RequestBody

- application/json

```ts
{
  // Serial operating mode
  serial1Mode: string
  // Baud rate
  serial1BaudRate: integer
  // Data bits
  serial1DataBits: string
  // Stop bits
  serial1StopBits: string
  // Parity
  serial1Parity: string
  serial2Mode:#/components/schemas/SerialMode
  // Baud rate
  serial2BaudRate: integer
  serial2DataBits:#/components/schemas/SerialDataBits
  serial2StopBits:#/components/schemas/SerialStopBits
  serial2Parity:#/components/schemas/SerialParity
  serial3Mode:#/components/schemas/SerialMode
  // Baud rate
  serial3BaudRate: integer
  serial3DataBits:#/components/schemas/SerialDataBits
  serial3StopBits:#/components/schemas/SerialStopBits
  serial3Parity:#/components/schemas/SerialParity
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Config updated"
}
```

- 400 Bad request

- 401 Not authenticated

***

### [GET]/config/tcpLogger

- Summary  
Get TCP logger config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Enable TCP logger on port 3000
  enabled: boolean
}
```

- 401 Not authenticated

***

### [POST]/config/tcpLogger

- Summary  
Set TCP logger config

#### RequestBody

- application/json

```ts
{
  // Enable TCP logger on port 3000
  enabled: boolean
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Config updated"
}
```

- 400 Bad request

- 401 Not authenticated

***

### [GET]/config/wifi

- Summary  
Get WiFi config

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Enable WiFi STA connection
  enabled: boolean
  // WiFi SSID
  ssid: string
  // Passport, empty string if not required
  password?: string
}
```

- 401 Not authenticated

***

### [POST]/config/wifi

- Summary  
Set WiFi config

#### RequestBody

- application/json

```ts
{
  // Charging current in A
  chargingCurrent?: number
  // Default charging current in A, stored in NVS
  defaultChargingCurrent?: number
  // Require authorization to start charging
  requireAuth?: boolean
  // Socket outlet functionality (proximity pilot sensing and socket lock if available)
  socketOutlet?: boolean
  // Residual current monitor
  rcm?: boolean
  // Session consumption limit in Ws
  consumptionLimit?: integer
  // Default session consumption limit in Ws, stored in NVS
  defaultConsumptionLimit?: integer
  // Session elapsed time limit in s
  elapsedLimit?: integer
  // Default session elapsed time limit in s, stored in NVS
  defaultElapsedLimit?: integer
  // Session underpower limit in W
  underPowerLimit?: integer
  // Default session underpower limit in W, stored in NVS
  defaultUnderPowerLimit?: integer
  // Socket lock operating time in ms
  socketLockOperatingTime?: integer
  // Socket lock break time in ms
  socketLockBreakTime?: integer
  // Socket lock detection (unlock_high=false, locked_high=true)
  socketLockDetectionHigh?: boolean
  // Socket lock retry count
  socketLockRetryCount?: integer
  // Energy meter mode
  energyMeter?: string
  // Energy meter voltage in V, when is not measured
  acVoltage?: integer
  // Energy meter pulse amount in Wh
  pulseAmount?: integer
  aux1?: string
  aux2?: string
  aux3?: string
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Config updated"
}
```

- 400 Bad request

- 401 Not authenticated

***

### [POST]/credentials

- Summary  
Set authorization credentials

#### RequestBody

- application/json

```ts
{
  // Username, empty string if not required
  user?: string
  // Password, empty string if not required
  password?: string
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Credentials updated"
}
```

- 400 Bad request

- 401 Not authenticated

***

### [POST]/firmware/checkUpdate

- Summary  
Check for new firmware version on OTA server

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Available firmware version
  available: string
  // Current firmware version
  current: string
  // Is avaialble newer version than current
  newer: boolean
}
```

- 400 Bad request

- 401 Not authenticated

- 500 Internal server error

***

### [POST]/firmware/update

- Summary  
Update firmware from OTA server

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Firmware upgraded successfully"
}
```

- 401 Not authenticated

- 500 Firmware upgrade failed

***

### [POST]/firmware/upload

- Summary  
Upload firmware

#### RequestBody

- application/octet-stream

```ts
{
  "type": "string",
  "format": "binary"
}
```

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Firmware uploaded successfully"
}
```

- 401 Not authenticated

- 500 Firmware upload failed

***

### [GET]/info

- Summary  
Get info

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  appDate: string
  appTime: string
  appVersion: string
  chip: string
  chipCores: integer
  chipRevision: number
  idfVersion: string
  ip: string
  ipAp: string
  mac: string
  macAp: string
  maxHeapSize: integer
  temperatureHigh: number
  temperatureLow: number
  temperatureSensorCount: integer
  // Uptime in s
  uptime: integer
}
```

- 401 Not authenticated

***

### [POST]/restart

- Summary  
Restart device

#### Responses

- 200 Successful operation

`text/plain`

```ts
{
  "type": "string",
  "example": "Restart in one second"
}
```

- 401 Not authenticated

***

### [GET]/state

- Summary  
Get state

#### Responses

- 200 Successful operation

`application/json`

```ts
{
  // Power in W
  actualPower: integer
  // Session consumption in Ws
  consumption: integer
  current?: number[]
  voltage?: number[]
  // Session elapsed time in s
  elapsed: integer
  // Charging enabled
  enabled: boolean
  // Non zero value if error occured
  error: integer
  // Session charging limit reached
  limitReached: boolean
  // Pending auhorization before start charging, when authorization is required
  pendingAuth: boolean
  // EVSE state
  state: string
}
```

- 401 Not authenticated

## References

### #/components/schemas/BoardConfig

```ts
{
  aux1: boolean
  aux2: boolean
  aux3: boolean
  deviceName: string
  energyMeter: string
  energyMeterThreePhases: boolean
  maxChargingCurrent: integer
  proximity: boolean
  rcm: boolean
  // Serial type
  serial1: string
  serial1Name: string
  serial2:#/components/schemas/SerialType
  serial2Name: string
  serial3:#/components/schemas/SerialType
  serial3Name: string
  socketLock: boolean
  socketLockMinBreakTime: integer
}
```

### #/components/schemas/Credentials

```ts
{
  // Username, empty string if not required
  user?: string
  // Password, empty string if not required
  password?: string
}
```

### #/components/schemas/EvseConfig

```ts
{
  // Charging current in A
  chargingCurrent?: number
  // Default charging current in A, stored in NVS
  defaultChargingCurrent?: number
  // Require authorization to start charging
  requireAuth?: boolean
  // Socket outlet functionality (proximity pilot sensing and socket lock if available)
  socketOutlet?: boolean
  // Residual current monitor
  rcm?: boolean
  // Session consumption limit in Ws
  consumptionLimit?: integer
  // Default session consumption limit in Ws, stored in NVS
  defaultConsumptionLimit?: integer
  // Session elapsed time limit in s
  elapsedLimit?: integer
  // Default session elapsed time limit in s, stored in NVS
  defaultElapsedLimit?: integer
  // Session underpower limit in W
  underPowerLimit?: integer
  // Default session underpower limit in W, stored in NVS
  defaultUnderPowerLimit?: integer
  // Socket lock operating time in ms
  socketLockOperatingTime?: integer
  // Socket lock break time in ms
  socketLockBreakTime?: integer
  // Socket lock detection (unlock_high=false, locked_high=true)
  socketLockDetectionHigh?: boolean
  // Socket lock retry count
  socketLockRetryCount?: integer
  // Energy meter mode
  energyMeter?: string
  // Energy meter voltage in V, when is not measured
  acVoltage?: integer
  // Energy meter pulse amount in Wh
  pulseAmount?: integer
  aux1?: string
  aux2?: string
  aux3?: string
}
```

### #/components/schemas/FirmwareCheck

```ts
{
  // Available firmware version
  available: string
  // Current firmware version
  current: string
  // Is avaialble newer version than current
  newer: boolean
}
```

### #/components/schemas/FullConfig

```ts
{
  evse: {
    // Charging current in A
    chargingCurrent?: number
    // Default charging current in A, stored in NVS
    defaultChargingCurrent?: number
    // Require authorization to start charging
    requireAuth?: boolean
    // Socket outlet functionality (proximity pilot sensing and socket lock if available)
    socketOutlet?: boolean
    // Residual current monitor
    rcm?: boolean
    // Session consumption limit in Ws
    consumptionLimit?: integer
    // Default session consumption limit in Ws, stored in NVS
    defaultConsumptionLimit?: integer
    // Session elapsed time limit in s
    elapsedLimit?: integer
    // Default session elapsed time limit in s, stored in NVS
    defaultElapsedLimit?: integer
    // Session underpower limit in W
    underPowerLimit?: integer
    // Default session underpower limit in W, stored in NVS
    defaultUnderPowerLimit?: integer
    // Socket lock operating time in ms
    socketLockOperatingTime?: integer
    // Socket lock break time in ms
    socketLockBreakTime?: integer
    // Socket lock detection (unlock_high=false, locked_high=true)
    socketLockDetectionHigh?: boolean
    // Socket lock retry count
    socketLockRetryCount?: integer
    // Energy meter mode
    energyMeter?: string
    // Energy meter voltage in V, when is not measured
    acVoltage?: integer
    // Energy meter pulse amount in Wh
    pulseAmount?: integer
    aux1?: string
    aux2?: string
    aux3?: string
  }
  modbus: {
    // Enable Modbus TCP/IP slave on port 502
    tcpEnabled: boolean
    // Modbus Unit ID
    unitId: integer
  }
  mqtt: {
    // Enable MQTT
    enabled: boolean
    // Server url
    server: string
    // Base topic
    baseTopic: string
    // User, empty string if not required
    user: string
    // Passport, empty string if not required
    password: string
    // Periodicity publishing state topic in s
    periodicity: integer
  }
  serial: {
    // Serial operating mode
    serial1Mode: string
    // Baud rate
    serial1BaudRate: integer
    // Data bits
    serial1DataBits: string
    // Stop bits
    serial1StopBits: string
    // Parity
    serial1Parity: string
    serial2Mode:#/components/schemas/SerialMode
    // Baud rate
    serial2BaudRate: integer
    serial2DataBits:#/components/schemas/SerialDataBits
    serial2StopBits:#/components/schemas/SerialStopBits
    serial2Parity:#/components/schemas/SerialParity
    serial3Mode:#/components/schemas/SerialMode
    // Baud rate
    serial3BaudRate: integer
    serial3DataBits:#/components/schemas/SerialDataBits
    serial3StopBits:#/components/schemas/SerialStopBits
    serial3Parity:#/components/schemas/SerialParity
  }
  tcpLogger: {
    // Enable TCP logger on port 3000
    enabled: boolean
  }
  wifi: {
    // Enable WiFi STA connection
    enabled: boolean
    // WiFi SSID
    ssid: string
    // Passport, empty string if not required
    password?: string
  }
}
```

### #/components/schemas/Info

```ts
{
  appDate: string
  appTime: string
  appVersion: string
  chip: string
  chipCores: integer
  chipRevision: number
  idfVersion: string
  ip: string
  ipAp: string
  mac: string
  macAp: string
  maxHeapSize: integer
  temperatureHigh: number
  temperatureLow: number
  temperatureSensorCount: integer
  // Uptime in s
  uptime: integer
}
```

### #/components/schemas/ModbusConfig

```ts
{
  // Enable Modbus TCP/IP slave on port 502
  tcpEnabled: boolean
  // Modbus Unit ID
  unitId: integer
}
```

### #/components/schemas/MqttConfig

```ts
{
  // Enable MQTT
  enabled: boolean
  // Server url
  server: string
  // Base topic
  baseTopic: string
  // User, empty string if not required
  user: string
  // Passport, empty string if not required
  password: string
  // Periodicity publishing state topic in s
  periodicity: integer
}
```

### #/components/schemas/SerialConfig

```ts
{
  // Serial operating mode
  serial1Mode: string
  // Baud rate
  serial1BaudRate: integer
  // Data bits
  serial1DataBits: string
  // Stop bits
  serial1StopBits: string
  // Parity
  serial1Parity: string
  serial2Mode:#/components/schemas/SerialMode
  // Baud rate
  serial2BaudRate: integer
  serial2DataBits:#/components/schemas/SerialDataBits
  serial2StopBits:#/components/schemas/SerialStopBits
  serial2Parity:#/components/schemas/SerialParity
  serial3Mode:#/components/schemas/SerialMode
  // Baud rate
  serial3BaudRate: integer
  serial3DataBits:#/components/schemas/SerialDataBits
  serial3StopBits:#/components/schemas/SerialStopBits
  serial3Parity:#/components/schemas/SerialParity
}
```

### #/components/schemas/SerialDataBits

```ts
{
  "type": "string",
  "description": "Data bits",
  "enum": [
    "5",
    "6",
    "7",
    "8"
  ]
}
```

### #/components/schemas/SerialMode

```ts
{
  "type": "string",
  "description": "Serial operating mode",
  "enum": [
    "none",
    "at",
    "log",
    "modbus"
  ]
}
```

### #/components/schemas/SerialParity

```ts
{
  "type": "string",
  "description": "Parity",
  "enum": [
    "disable",
    "even",
    "odd"
  ]
}
```

### #/components/schemas/SerialStopBits

```ts
{
  "type": "string",
  "description": "Stop bits",
  "enum": [
    "1",
    "1.5",
    "2"
  ]
}
```

### #/components/schemas/SerialType

```ts
{
  "type": "string",
  "description": "Serial type",
  "enum": [
    "none",
    "uart",
    "rs485"
  ]
}
```

### #/components/schemas/State

```ts
{
  // Power in W
  actualPower: integer
  // Session consumption in Ws
  consumption: integer
  current?: number[]
  voltage?: number[]
  // Session elapsed time in s
  elapsed: integer
  // Charging enabled
  enabled: boolean
  // Non zero value if error occured
  error: integer
  // Session charging limit reached
  limitReached: boolean
  // Pending auhorization before start charging, when authorization is required
  pendingAuth: boolean
  // EVSE state
  state: string
}
```

### #/components/schemas/TcpLoggerConfig

```ts
{
  // Enable TCP logger on port 3000
  enabled: boolean
}
```

### #/components/schemas/WifiConfig

```ts
{
  // Enable WiFi STA connection
  enabled: boolean
  // WiFi SSID
  ssid: string
  // Passport, empty string if not required
  password?: string
}
```

### #/components/securitySchemes/basicAuth

```ts
{
  "type": "http",
  "scheme": "basic"
}
```