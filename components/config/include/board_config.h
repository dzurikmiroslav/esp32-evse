#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

#include <soc/soc_caps.h>
#include <stdbool.h>
#include <stdint.h>

#define BOARD_CFG_DEVICE_NAME_SIZE       32
#define BOARD_CFG_AUX_NAME_SIZE          8
#define BOARD_CFG_AUX_INPUT_COUNT        4
#define BOARD_CFG_AUX_OUTPUT_COUNT       4
#define BOARD_CFG_AUX_ANALOG_INPUT_COUNT 2
#define BOARD_CFG_SERIAL_NAME_SIZE       16
#define BOARD_CFG_SERIAL_COUNT           SOC_UART_NUM
#define BOARD_CFG_OTA_CHANNEL_COUNT      3
#define BOARD_CFG_OTA_CHANNEL_NAME_SIZE  16

#define board_cfg_is_proximity(config)      (config.proximity.adc_channel != -1)
#define board_cfg_is_ac_relay_l2_l3(config) (config.ac_relay.gpios[BOARD_CFG_AC_RELAY_GPIO_L2_L3] != -1)
#define board_cfg_is_socket_lock(config) \
    (config.socket_lock.gpios[BOARD_CFG_SOCKET_LOCK_GPIO_A] != -1 && config.socket_lock.gpios[BOARD_CFG_SOCKET_LOCK_GPIO_B] != -1 && config.socket_lock.detection_gpio != -1)
#define board_cfg_is_rcm(config)                   (config.rcm.gpio != -1 && config.rcm.test_gpio != -1)
#define board_cfg_is_aux_input(config, idx)        (config.aux.inputs[idx].gpio != -1)
#define board_cfg_is_aux_output(config, idx)       (config.aux.outputs[idx].gpio != -1)
#define board_cfg_is_aux_analog_input(config, idx) (config.aux.analog_inputs[idx].adc_channel != -1)
#define board_cfg_is_onewire(config)               (config.onewire.gpio != -1)
#define board_cfg_is_energy_meter_cur(config)      (config.energy_meter.cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L1] != -1)
#define board_cfg_is_energy_meter_vlt(config)      (config.energy_meter.vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L1] != -1)
#define board_cfg_is_energy_meter_cur_3p(config) \
    (config.energy_meter.cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L2] != -1 && config.energy_meter.cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L3] != -1)
#define board_cfg_is_energy_meter_vlt_3p(config) \
    (config.energy_meter.vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L2] != -1 && config.energy_meter.vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_L3] != -1)
#define board_cfg_is_ota_channel(config, idx) (config.ota.channels[idx].path != NULL)

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

typedef enum {
    BOARD_CFG_AC_RELAY_GPIO_L1,
    BOARD_CFG_AC_RELAY_GPIO_L2_L3,
    BOARD_CFG_AC_RELAY_GPIO_MAX
} board_cfg_ac_relay_gpio_t;

typedef enum {
    BOARD_CFG_SOCKET_LOCK_GPIO_A,
    BOARD_CFG_SOCKET_LOCK_GPIO_B,
    BOARD_CFG_SOCKET_LOCK_GPIO_MAX
} board_cfg_socket_lock_gpio_t;

typedef struct {
    char name[BOARD_CFG_OTA_CHANNEL_NAME_SIZE];
    const char* path;
} board_cfg_ota_channel_t;

typedef struct {
    char name[BOARD_CFG_AUX_NAME_SIZE];
    int8_t gpio;
} board_cfg_aux_input_output_t;

typedef struct {
    char name[BOARD_CFG_AUX_NAME_SIZE];
    int8_t adc_channel;
} board_cfg_aux_analog_input_t;

typedef enum {
    BOARD_CFG_SERIAL_TYPE_NONE,
    BOARD_CFG_SERIAL_TYPE_UART,
    BOARD_CFG_SERIAL_TYPE_RS485
} board_cfg_serial_type_t;

typedef struct {
    board_cfg_serial_type_t type;
    char name[BOARD_CFG_SERIAL_NAME_SIZE];
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
    char device_name[BOARD_CFG_DEVICE_NAME_SIZE];

    struct leds_s {
        int8_t charging_gpio;
        int8_t error_gpio;
        int8_t wifi_gpio;
    } leds;

    struct button_s {
        int8_t gpio;
    } button;

    struct pilot_s {
        int8_t gpio;
        int8_t adc_channel;
        uint16_t levels[BOARD_CFG_PILOT_LEVEL_MAX];
    } pilot;

    struct proximity_s {
        int8_t adc_channel;
        uint16_t levels[BOARD_CFG_PROXIMITY_LEVEL_MAX];
    } proximity;

    struct ac_relay_s {
        int8_t gpios[BOARD_CFG_AC_RELAY_GPIO_MAX];
    } ac_relay;

    struct socket_lock_s {
        int8_t gpios[BOARD_CFG_SOCKET_LOCK_GPIO_MAX];
        int8_t detection_gpio;
        uint16_t detection_delay;
        uint16_t min_break_time;
    } socket_lock;

    struct rcm_s {
        int8_t gpio;
        int8_t test_gpio;
    } rcm;

    struct energy_meter_s {
        int8_t cur_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX];
        float cur_scale;

        int8_t vlt_adc_channel[BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX];
        float vlt_scale;
    } energy_meter;

    struct aux_s {
        board_cfg_aux_input_output_t inputs[BOARD_CFG_AUX_INPUT_COUNT];
        board_cfg_aux_input_output_t outputs[BOARD_CFG_AUX_OUTPUT_COUNT];
        board_cfg_aux_analog_input_t analog_inputs[BOARD_CFG_AUX_ANALOG_INPUT_COUNT];
    } aux;

    board_cfg_serial_t serials[BOARD_CFG_SERIAL_COUNT];

    struct onewire_s {
        int8_t gpio;
        bool temp_sensor : 1;
    } onewire;

    struct ota_s {
        board_cfg_ota_channel_t channels[BOARD_CFG_OTA_CHANNEL_COUNT];
    } ota;
} board_cfg_t;

extern board_cfg_t board_config;

void board_config_load(bool reset);

#endif /* BOARD_CONFIG_H_ */
