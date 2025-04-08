#include "energy_meter.h"

uint16_t energy_meter_mock_power = 0;

uint32_t energy_meter_mock_charging_time = 0;

uint32_t energy_meter_mock_consumption = 0;

void energy_meter_init(void)
{}

void energy_meter_process(bool charging, uint16_t charging_current)
{}

void energy_meter_start_session(void)
{}

void energy_meter_stop_session(void)
{}

uint16_t energy_meter_get_power(void)
{
    return energy_meter_mock_power;
}

uint32_t energy_meter_get_session_time(void)
{
    return 0;
}

uint32_t energy_meter_get_charging_time(void)
{
    return energy_meter_mock_charging_time;
}

uint32_t energy_meter_get_consumption(void)
{
    return energy_meter_mock_consumption;
}

void energy_meter_get_voltage(float* voltage)
{
    voltage[0] = energy_meter_get_l1_voltage();
    voltage[1] = energy_meter_get_l2_voltage();
    voltage[2] = energy_meter_get_l3_voltage();
}

float energy_meter_get_l1_voltage(void)
{
    return 251;
}

float energy_meter_get_l2_voltage(void)
{
    return 252;
}

float energy_meter_get_l3_voltage(void)
{
    return 253;
}

void energy_meter_get_current(float* current)
{
    current[0] = energy_meter_get_l1_current();
    current[1] = energy_meter_get_l2_current();
    current[2] = energy_meter_get_l3_current();
}

float energy_meter_get_l1_current(void)
{
    return 16.1;
}

float energy_meter_get_l2_current(void)
{
    return 16.2;
}

float energy_meter_get_l3_current(void)
{
    return 16.3;
}

energy_meter_mode_t energy_meter_get_mode(void)
{
    return ENERGY_METER_MODE_CUR_VLT;
}

uint16_t energy_meter_get_ac_voltage(void)
{
    return 255;
}

bool energy_meter_is_three_phases(void)
{
    return true;
}