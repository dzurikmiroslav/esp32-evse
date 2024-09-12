#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

#include <hal/adc_types.h>
#include <hal/gpio_types.h>
#include <soc/soc_caps.h>

typedef enum {
    BOARD_CONFIG_ENERGY_METER_NONE,
    BOARD_CONFIG_ENERGY_METER_CUR,
    BOARD_CONFIG_ENERGY_METER_CUR_VLT
} board_config_energy_meter_t;

typedef enum {
    BOARD_CONFIG_SERIAL_NONE,
    BOARD_CONFIG_SERIAL_UART,
    BOARD_CONFIG_SERIAL_RS485
} board_config_serial_t;

typedef struct {
    char device_name[32];

    bool led_charging : 1;
    gpio_num_t led_charging_gpio;
    bool led_error : 1;
    gpio_num_t led_error_gpio;
    bool led_wifi : 1;
    gpio_num_t led_wifi_gpio;

    gpio_num_t button_wifi_gpio;

    gpio_num_t pilot_pwm_gpio;
    adc_channel_t pilot_adc_channel;
    uint16_t pilot_down_threshold_12;
    uint16_t pilot_down_threshold_9;
    uint16_t pilot_down_threshold_6;
    uint16_t pilot_down_threshold_3;
    uint16_t pilot_down_threshold_n12;

    bool proximity : 1;
    adc_channel_t proximity_adc_channel;
    uint16_t proximity_down_threshold_13;
    uint16_t proximity_down_threshold_20;
    uint16_t proximity_down_threshold_32;

    gpio_num_t ac_relay_gpio;

    bool socket_lock : 1;
    gpio_num_t socket_lock_a_gpio;
    gpio_num_t socket_lock_b_gpio;
    gpio_num_t socket_lock_detection_gpio;
    uint16_t socket_lock_detection_delay;
    uint16_t socket_lock_min_break_time;

    bool rcm : 1;
    gpio_num_t rcm_gpio;
    gpio_num_t rcm_test_gpio;

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

    bool aux_in_1 : 1;
    char aux_in_1_name[8];
    gpio_num_t aux_in_1_gpio;

    bool aux_in_2 : 1;
    char aux_in_2_name[8];
    gpio_num_t aux_in_2_gpio;

    bool aux_in_3 : 1;
    char aux_in_3_name[8];
    gpio_num_t aux_in_3_gpio;

    bool aux_in_4 : 1;
    char aux_in_4_name[8];
    gpio_num_t aux_in_4_gpio;

    bool aux_out_1 : 1;
    char aux_out_1_name[8];
    gpio_num_t aux_out_1_gpio;

    bool aux_out_2 : 1;
    char aux_out_2_name[8];
    gpio_num_t aux_out_2_gpio;

    bool aux_out_3 : 1;
    char aux_out_3_name[8];
    gpio_num_t aux_out_3_gpio;

    bool aux_out_4 : 1;
    char aux_out_4_name[8];
    gpio_num_t aux_out_4_gpio;

    bool aux_ain_1 : 1;
    char aux_ain_1_name[8];
    adc_channel_t aux_ain_1_adc_channel;

    bool aux_ain_2 : 1;
    char aux_ain_2_name[8];
    adc_channel_t aux_ain_2_adc_channel;

    board_config_serial_t serial_1;
    char serial_1_name[16];
    gpio_num_t serial_1_rxd_gpio;
    gpio_num_t serial_1_txd_gpio;
    gpio_num_t serial_1_rts_gpio;
    board_config_serial_t serial_2;
    char serial_2_name[16];
    gpio_num_t serial_2_rxd_gpio;
    gpio_num_t serial_2_txd_gpio;
    gpio_num_t serial_2_rts_gpio;
#if SOC_UART_NUM > 2
    board_config_serial_t serial_3;
    char serial_3_name[16];
    gpio_num_t serial_3_rxd_gpio;
    gpio_num_t serial_3_txd_gpio;
    gpio_num_t serial_3_rts_gpio;
#endif /* SOC_UART_NUM > 2 */

    bool onewire : 1;
    gpio_num_t onewire_gpio;
    bool onewire_temp_sensor : 1;
} board_config_t;

extern board_config_t board_config;

void board_config_load();

#endif /* BOARD_CONFIG_H_ */
