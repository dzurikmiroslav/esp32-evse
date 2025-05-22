#include "energy_meter.h"

energy_meter_mode_t energy_meter_mock_mode = ENERGY_METER_MODE_DUMMY;

uint16_t energy_meter_ac_voltage = 255;

uint16_t energy_meter_mock_power = 0;

uint32_t energy_meter_mock_charging_time = 0;

uint32_t energy_meter_mock_consumption = 0;

uint64_t energy_meter_mock_total_consumption = 1000;

bool energy_meter_three_phases = true;

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

uint64_t energy_meter_get_total_consumption(void)
{
    return energy_meter_mock_total_consumption + energy_meter_mock_consumption / 3600;
}

void energy_meter_reset_total_consumption(void)
{
    energy_meter_mock_total_consumption = 0;
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
    return energy_meter_mock_mode;
}

esp_err_t energy_meter_set_mode(energy_meter_mode_t mode)
{
    energy_meter_mock_mode = mode;
    return ESP_OK;
}

uint16_t energy_meter_get_ac_voltage(void)
{
    return energy_meter_ac_voltage;
}

esp_err_t energy_meter_set_ac_voltage(uint16_t _ac_voltage)
{
    if (_ac_voltage < 100 || _ac_voltage > 300) {
        return ESP_ERR_INVALID_ARG;
    }

    energy_meter_ac_voltage = _ac_voltage;

    return ESP_OK;
}

bool energy_meter_is_three_phases(void)
{
    return energy_meter_three_phases;
}

void energy_meter_set_three_phases(bool _three_phases)
{
    energy_meter_three_phases = _three_phases;
}
