#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

#include "driver/gpio.h"
#include "driver/adc.h"

typedef enum {
    BOARD_CONFIG_ENERGY_METER_NONE,
    BOARD_CONFIG_ENERGY_METER_CUR,
    BOARD_CONFIG_ENERGY_METER_CUR_VLT,
    BOARD_CONFIG_ENERGY_METER_EXT_PULSE //TODO remove
} board_config_energy_meter_t;

typedef struct
{
    bool led_charging : 1;
    gpio_num_t led_charging_gpio;
    bool led_error : 1;
    gpio_num_t led_error_gpio;
    bool led_wifi : 1;
    gpio_num_t led_wifi_gpio;

    gpio_num_t button_wifi_gpio;

    gpio_num_t pilot_pwm_gpio;
    adc_channel_t pilot_sens_adc_channel;
    uint16_t pilot_sens_down_treshold_12;
    uint16_t pilot_sens_down_treshold_9;
    uint16_t pilot_sens_down_treshold_6;
    uint16_t pilot_sens_down_treshold_3;
    uint16_t pilot_sens_down_treshold_n12;

    uint8_t max_charging_current;

    gpio_num_t ac_relay_gpio;

    bool cable_lock : 1;
    gpio_num_t cable_lock_a_gpio;
    gpio_num_t cable_lock_b_gpio;
    gpio_num_t cable_lock_test_gpio;

    board_config_energy_meter_t energy_meter;
    bool energy_meter_three_phases : 1;

    adc_channel_t energy_meter_l1_cur_adc_channel;
    adc_channel_t energy_meter_l2_cur_adc_channel;
    adc_channel_t energy_meter_l3_cur_adc_channel;
    float energy_meter_cur_scale;
    adc_channel_t energy_meter_l1_vlt_adc_channel;
    adc_channel_t energy_meter_l2_vlt_adc_channel;
    adc_channel_t energy_meter_l3_vlt_adc_channel;
    float energy_meter_vlt_scale;

    gpio_num_t energy_meter_ext_pulse_gpio;

    bool aux_1 : 1;
    gpio_num_t aux_1_gpio;
    bool aux_2 : 1;
    gpio_num_t aux_2_gpio;
    bool aux_3 : 1;
    gpio_num_t aux_3_gpio;
} board_config_t;

extern board_config_t board_config;

void board_config_load();

#endif /* BOARD_CONFIG_H_ */
