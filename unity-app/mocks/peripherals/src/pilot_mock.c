#include "peripherals_mock.h"
#include "pilot.h"

pilot_voltage_t pilot_mock_up_voltage;

bool pilot_mock_down_voltage_n12;

pilot_mock_state_t pilot_mock_state;

void pilot_init(void)
{}

void pilot_set_level(bool level)
{
    pilot_mock_state = level ? PILOT_MOCK_STATE_HIGH : PILOT_MOCK_STATE_LOW;
}

void pilot_set_amps(uint16_t amps)
{
    pilot_mock_state = PILOT_MOCK_STATE_PWM;
}

void pilot_measure(pilot_voltage_t *up_voltage, bool *down_voltage_n12)
{
    *up_voltage = pilot_mock_up_voltage;
    *down_voltage_n12 = pilot_mock_down_voltage_n12;
}