#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

#include "driver/gpio.h"
#include "driver/adc.h"

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

    gpio_num_t ac_relay_gpio;
} board_config_t;

extern board_config_t board_config;

void board_config_load();

#endif /* BOARD_CONFIG_H_ */
