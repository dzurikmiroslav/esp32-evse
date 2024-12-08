#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

#include <soc/soc_caps.h>
#include <stdbool.h>
#include <stdint.h>

#define BOARD_CFG_DEVICE_NAME_LEN        32
#define BOARD_CFG_AUX_NAME_LEN           8
#define BOARD_CFG_AUX_INPUT_COUNT        4
#define BOARD_CFG_AUX_OUTPUT_COUNT       4
#define BOARD_CFG_AUX_ANALOG_INPUT_COUNT 2
#define BOARD_CFG_SERIAL_NAME_LEN        16
#define BOARD_CFG_SERIAL_COUNT           SOC_UART_NUM

#define board_cfg_is_proximity(config)             (config.proximity_adc_channel != -1)
#define board_cfg_is_socket_lock(config)           (config.socket_lock_a_gpio != -1 && config.socket_lock_b_gpio != -1 && config.socket_lock_detection_gpio != -1)
#define board_cfg_is_rcm(config)                   (config.rcm_gpio != -1 && config.rcm_test_gpio != -1)
#define board_cfg_is_aux_input(config, idx)        (config.aux_inputs[idx].gpio != -1)
#define board_cfg_is_aux_output(config, idx)       (config.aux_outputs[idx].gpio != -1)
#define board_cfg_is_aux_analog_input(config, idx) (config.aux_analog_inputs[idx].adc_channel != -1)
#define board_cfg_is_onewire(config)               (config.onewire_gpio != -1)
#define board_cfg_is_energy_meter_cur(config)      (config.energy_meter_cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L1] != -1)
#define board_cfg_is_energy_meter_vlt(config)      (config.energy_meter_vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L1] != -1)
#define board_cfg_is_energy_meter_cur_3p(config) \
    (config.energy_meter_cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L2] != -1 && config.energy_meter_cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L3] != -1)
#define board_cfg_is_energy_meter_vlt_3p(config) \
    (config.energy_meter_vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L2] != -1 && config.energy_meter_vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L3] != -1)

typedef enum {
    BOARD_CFG_PILOT_LEVEL_12,
    BOARD_CFG_PILOT_LEVEL_9,
    BOARD_CFG_PILOT_LEVEL_6,
    BOARD_CFG_PILOT_LEVEL_3,
    BOARD_CFG_PILOT_LEVEL_N12,
    BOARD_CFG_PILOT_LEVEL_MAX
} board_cfg_pilot_level_t;

typedef enum {
    BOARD_CFG_PROXIMITY_LEVEL_13,
    BOARD_CFG_PROXIMITY_LEVEL_20,
    BOARD_CFG_PROXIMITY_LEVEL_32,
    BOARD_CFG_PROXIMITY_LEVEL_MAX
} board_cfg_proximity_level_t;

typedef struct {
    char name[BOARD_CFG_AUX_NAME_LEN];
    int8_t gpio;
} board_cfg_aux_input_output_t;

typedef struct {
    char name[BOARD_CFG_AUX_NAME_LEN];
    int8_t adc_channel;
} board_cfg_aux_analog_input_t;

typedef enum {
    BOARD_CFG_SERIAL_TYPE_NONE,
    BOARD_CFG_SERIAL_TYPE_UART,
    BOARD_CFG_SERIAL_TYPE_RS485
} board_cfg_serial_type_t;

typedef struct {
    board_cfg_serial_type_t type;
    char name[BOARD_CFG_SERIAL_NAME_LEN];
    int8_t rxd_gpio;
    int8_t txd_gpio;
    int8_t rts_gpio;
} board_cfg_serial_t;

typedef enum {
    BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L1,
    BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L2,
    BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L3,
    BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX
} board_cfg_energy_meter_adc_channel_t;

typedef struct {
    char device_name[BOARD_CFG_DEVICE_NAME_LEN];

    int8_t led_charging_gpio;
    int8_t led_error_gpio;
    int8_t led_wifi_gpio;

    int8_t button_gpio;

    int8_t pilot_gpio;
    int8_t pilot_adc_channel;
    uint16_t pilot_levels[BOARD_CFG_PILOT_LEVEL_MAX];

    int8_t proximity_adc_channel;
    uint16_t proximity_levels[BOARD_CFG_PROXIMITY_LEVEL_MAX];

    int8_t ac_relay_gpio;

    int8_t socket_lock_a_gpio;
    int8_t socket_lock_b_gpio;
    int8_t socket_lock_detection_gpio;
    uint16_t socket_lock_detection_delay;
    uint16_t socket_lock_min_break_time;

    int8_t rcm_gpio;
    int8_t rcm_test_gpio;

    int8_t energy_meter_cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX];
    float energy_meter_cur_scale;

    int8_t energy_meter_vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX];
    float energy_meter_vlt_scale;

    board_cfg_aux_input_output_t aux_inputs[BOARD_CFG_AUX_INPUT_COUNT];

    board_cfg_aux_input_output_t aux_outputs[BOARD_CFG_AUX_OUTPUT_COUNT];

    board_cfg_aux_analog_input_t aux_analog_inputs[BOARD_CFG_AUX_ANALOG_INPUT_COUNT];

    board_cfg_serial_t serials[BOARD_CFG_SERIAL_COUNT];

    int8_t onewire_gpio;
    bool onewire_temp_sensor : 1;
} board_cfg_t;

extern board_cfg_t board_config;

void board_config_load(void);

#endif /* BOARD_CONFIG_H_ */
