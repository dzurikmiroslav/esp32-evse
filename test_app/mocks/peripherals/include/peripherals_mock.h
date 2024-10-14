#ifndef PERIPHERALS_MOCK_H_
#define PERIPHERALS_MOCK_H_

#include "pilot.h"

// pilot

extern pilot_voltage_t pilot_mock_up_voltage;

extern bool pilot_mock_down_voltage_n12;

typedef enum {
    PILOT_MOCK_STATE_HIGH,
    PILOT_MOCK_STATE_LOW,
    PILOT_MOCK_STATE_PWM
} pilot_mock_state_t;

extern pilot_mock_state_t pilot_mock_state;

// ac relay

extern bool ac_relay_mock_state;

// energy meter

extern uint16_t energy_meter_mock_power;

extern uint32_t energy_meter_mock_charging_time;

extern uint32_t energy_meter_mock_consumption;

#endif /* PERIPHERALS_MOCK_H_ */