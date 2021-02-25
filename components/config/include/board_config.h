#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

#include "driver/gpio.h"
#include "driver/adc.h"

typedef enum {
    BOARD_CONFIG_ENERGY_METER_NONE,
    BOARD_CONFIG_ENERGY_METER_INTERNAL,
    BOARD_CONFIG_ENERGY_METER_EXTERNAL_PULSE
} board_config_energy_meter_t;

typedef struct
{
    bool led_charging :1;
    gpio_num_t led_charging_gpio;
    bool led_error :1;
    gpio_num_t led_error_gpio;
    bool led_wifi :1;
    gpio_num_t led_wifi_gpio;

    gpio_num_t button_wifi_gpio;

    gpio_num_t pilot_pwm_gpio;
    adc_channel_t pilot_sens_adc_channel;
    uint16_t pilot_sens_down_treshold_12;
    uint16_t pilot_sens_down_treshold_9;
    uint16_t pilot_sens_down_treshold_6;
    uint16_t pilot_sens_down_treshold_3;

    uint8_t max_charging_current;

    gpio_num_t ac_relay_gpio;

    bool cable_lock: 1;
    gpio_num_t cable_lock_b_gpio;
    gpio_num_t cable_lock_r_gpio;
    gpio_num_t cable_lock_w_gpio;

    board_config_energy_meter_t energy_meter;

    uint8_t energy_meter_internal_num_phases;
    adc_channel_t energy_meter_internal_l1_cur_adc_channel;
    adc_channel_t energy_meter_internal_l2_cur_adc_channel;
    adc_channel_t energy_meter_internal_l3_cur_adc_channel;
    adc_channel_t energy_meter_internal_l1_vlt_adc_channel;
    adc_channel_t energy_meter_internal_l2_vlt_adc_channel;
    adc_channel_t energy_meter_internal_l3_vlt_adc_channel;
    float energy_meter_internal_cur_scale;
    float energy_meter_internal_vlt_scale;

    bool energy_meter_external_pulse_gpio;
    float energy_meter_external_pulse_amount;
} board_config_t;

extern board_config_t board_config;

void board_config_load();

#endif /* BOARD_CONFIG_H_ */
