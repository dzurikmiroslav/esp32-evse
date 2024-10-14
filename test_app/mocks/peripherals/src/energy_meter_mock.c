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
{}

float energy_meter_get_l1_voltage(void)
{
    return 0;
}

float energy_meter_get_l2_voltage(void)
{
    return 0;
}

float energy_meter_get_l3_voltage(void)
{
    return 0;
}

void energy_meter_get_current(float* current)
{}

float energy_meter_get_l1_current(void)
{
    return 0;
}

float energy_meter_get_l2_current(void)
{
    return 0;
}

float energy_meter_get_l3_current(void)
{
    return 0;
}