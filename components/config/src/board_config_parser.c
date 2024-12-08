
#include "board_config_parser.h"

#include <esp_log.h>

#include "yaml.h"

#define YAML_MAX_LEVEL 2

#define CASE_SET_VALUE(level, prop, convert_fn) \
    case level:                                 \
        config->prop = convert_fn(value);       \
        break;

#define CASE_SET_VALUE_STR(level, prop, len) \
    case level:                              \
        strncpy(config->prop, value, len);   \
        break;

#define CASE_SET_SEQ_VALUE(level, prop, convert_fn, condition) \
    case level:                                                \
        if (condition) config->prop = convert_fn(value);       \
        break;

#define CASE_SET_SEQ_VALUE_STR(level, prop, len, condition) \
    case level:                                             \
        if (condition) strncpy(config->prop, value, len);   \
        break;

static const char* TAG = "board_config_parser";

static uint8_t str_to_gpio(const char* value)
{
    if (strlen(value)) {
        return atoi(value);
    } else {
        return -1;
    }
}

static bool str_to_bool(const char* value)
{
    return !strcmp(value, "true");
}

static board_cfg_serial_type_t str_to_serial_type(const char* value)
{
    if (!strcmp(value, "uart")) {
        return BOARD_CFG_SERIAL_TYPE_UART;
    }
    if (!strcmp(value, "rs485")) {
        return BOARD_CFG_SERIAL_TYPE_RS485;
    }
    return BOARD_CFG_SERIAL_TYPE_NONE;
}

typedef enum {
    KEY_INVALID = -2,
    KEY_NONE = -1,
    //
    KEY_DEVICE_NAME,
    KEY_LED_CHARGING_GPIO,
    KEY_LED_ERROR_GPIO,
    KEY_LED_WIFI_GPIO,
    KEY_BUTTON_GPIO,
    KEY_AC_RELAY_GPIO,
    KEY_PILOT,
    KEY_PROXIMITY,
    KEY_SOCKET_LOCK,
    KEY_RCM,
    KEY_AUX_IN,
    KEY_AUX_OUT,
    KEY_AUX_ANALOG_IN,
    KEY_GPIO,
    KEY_NAME,
    KEY_ADC_CHANNEL,
    KEY_LEVELS,
    KEY_A_GPIO,
    KEY_B_GPIO,
    KEY_DETECTION_GPIO,
    KEY_DETECTION_DELAY,
    KEY_MIN_BREAK_TIME,
    KEY_TEST_GPIO,
    KEY_ENERGY_METER,
    KEY_CURRENT_ADC_CHANNELS,
    KEY_VOLTAGE_ADC_CHANNELS,
    KEY_CURRENT_SCALE,
    KEY_VOLTAGE_SCALE,
    KEY_SERIAL,
    KEY_TYPE,
    KEY_RXD_GPIO,
    KEY_TXD_GPIO,
    KEY_RTS_GPIO,
    KEY_ONEWIRE,
    KEY_TEMPERATURE_SENSOR,
    //
    KEY_MAX,
} key_t;

// string values of key_t, must match key_t order
static const char* keys[] = {
    "deviceName",
    "ledChargingGpio",
    "ledErrorGpio",
    "ledWifiGpio",
    "buttonGpio",
    "acRelayGpio",
    "pilot",
    "proximity",
    "socketLock",
    "rcm",
    "auxInputs",
    "auxOutputs",
    "auxAnalogInputs",
    "gpio",
    "name",
    "adcChannel",
    "levels",
    "aGpio",
    "bGpio",
    "detectionGpio",
    "detectionDelay",
    "minBreakTime",
    "testGpio",
    "energyMeter",
    "currentAdcChannels",
    "voltageAdcChannels",
    "currentScale",
    "voltageScale",
    "serials",
    "type",
    "rxdGpio",
    "txdGpio",
    "rtsGpio",
    "onewire",
    "temperatureSensor",
};

static key_t get_key(const char* key)
{
    for (key_t i = KEY_NONE + 1; i < KEY_MAX; i++) {
        if (!strcmp(key, keys[i])) return i;
    }
    return KEY_NONE;
}

static bool set_key_value(board_cfg_t* config, const key_t* key, const int* seq_idx, const char* value)
{
    switch (key[0]) {
        CASE_SET_VALUE_STR(KEY_DEVICE_NAME, device_name, BOARD_CFG_DEVICE_NAME_LEN);
        CASE_SET_VALUE(KEY_LED_CHARGING_GPIO, led_charging_gpio, str_to_gpio);
        CASE_SET_VALUE(KEY_LED_ERROR_GPIO, led_error_gpio, str_to_gpio);
        CASE_SET_VALUE(KEY_LED_WIFI_GPIO, led_wifi_gpio, str_to_gpio);
        CASE_SET_VALUE(KEY_BUTTON_GPIO, button_gpio, str_to_gpio);
        CASE_SET_VALUE(KEY_AC_RELAY_GPIO, ac_relay_gpio, str_to_gpio);
    case KEY_PILOT:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_GPIO, pilot_gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_ADC_CHANNEL, pilot_adc_channel, str_to_gpio);
            CASE_SET_SEQ_VALUE(KEY_LEVELS, pilot_levels[seq_idx[1]], atoi, seq_idx[1] < BOARD_CFG_PILOT_LEVEL_MAX);
        default:
            return false;
        }
        break;
    case KEY_PROXIMITY:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_ADC_CHANNEL, proximity_adc_channel, str_to_gpio);
            CASE_SET_SEQ_VALUE(KEY_LEVELS, proximity_levels[seq_idx[1]], atoi, seq_idx[1] < BOARD_CFG_PROXIMITY_LEVEL_MAX);
        default:
            return false;
        }
        break;
    case KEY_SOCKET_LOCK:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_A_GPIO, socket_lock_a_gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_B_GPIO, socket_lock_b_gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_DETECTION_GPIO, socket_lock_detection_gpio, atoi);
            CASE_SET_VALUE(KEY_DETECTION_DELAY, socket_lock_detection_delay, atoi);
            CASE_SET_VALUE(KEY_MIN_BREAK_TIME, socket_lock_min_break_time, atoi);
        default:
            return false;
        }
        break;
    case KEY_ENERGY_METER:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE(KEY_CURRENT_ADC_CHANNELS, energy_meter_cur_adc_channel[seq_idx[1]], str_to_gpio, seq_idx[1] < BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX);
            CASE_SET_SEQ_VALUE(KEY_VOLTAGE_ADC_CHANNELS, energy_meter_vlt_adc_channel[seq_idx[1]], str_to_gpio, seq_idx[1] < BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX);
            CASE_SET_VALUE(KEY_CURRENT_SCALE, energy_meter_cur_scale, atof);
            CASE_SET_VALUE(KEY_VOLTAGE_SCALE, energy_meter_vlt_scale, atof);
        default:
            return false;
        }
        break;
    case KEY_RCM:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_GPIO, rcm_gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_TEST_GPIO, rcm_test_gpio, str_to_gpio);
        default:
            return false;
        }
        break;
    case KEY_AUX_IN:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE_STR(KEY_NAME, aux_inputs[seq_idx[0]].name, BOARD_CFG_AUX_NAME_LEN, seq_idx[0] < BOARD_CFG_AUX_INPUT_COUNT);
            CASE_SET_SEQ_VALUE(KEY_GPIO, aux_inputs[seq_idx[0]].gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_AUX_INPUT_COUNT);
        default:
            return false;
        }
        break;
    case KEY_AUX_OUT:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE_STR(KEY_NAME, aux_outputs[seq_idx[0]].name, BOARD_CFG_AUX_NAME_LEN, seq_idx[0] < BOARD_CFG_AUX_OUTPUT_COUNT);
            CASE_SET_SEQ_VALUE(KEY_GPIO, aux_outputs[seq_idx[0]].gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_AUX_OUTPUT_COUNT);
        default:
            return false;
        }
        break;
    case KEY_AUX_ANALOG_IN:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE_STR(KEY_NAME, aux_analog_inputs[seq_idx[0]].name, BOARD_CFG_AUX_NAME_LEN, seq_idx[0] < BOARD_CFG_AUX_ANALOG_INPUT_COUNT);
            CASE_SET_SEQ_VALUE(KEY_ADC_CHANNEL, aux_analog_inputs[seq_idx[0]].adc_channel, str_to_gpio, seq_idx[0] < BOARD_CFG_AUX_ANALOG_INPUT_COUNT);
        default:
            return false;
        }
        break;
    case KEY_SERIAL:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE(KEY_TYPE, serials[seq_idx[0]].type, str_to_serial_type, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE_STR(KEY_NAME, serials[seq_idx[0]].name, BOARD_CFG_SERIAL_NAME_LEN, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE(KEY_RXD_GPIO, serials[seq_idx[0]].rxd_gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE(KEY_TXD_GPIO, serials[seq_idx[0]].txd_gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE(KEY_RTS_GPIO, serials[seq_idx[0]].rts_gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
        default:
            return false;
        }
        break;
    case KEY_ONEWIRE:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_GPIO, onewire_gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_TEMPERATURE_SENSOR, onewire_temp_sensor, str_to_bool);
        default:
            return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static void empty_config(board_cfg_t* config)
{
    memset(config, 0, sizeof(board_cfg_t));
    config->led_charging_gpio = -1;
    config->led_error_gpio = -1;
    config->led_wifi_gpio = -1;
    config->button_gpio = -1;
    config->pilot_gpio = -1;
    config->pilot_adc_channel = -1;
    config->proximity_adc_channel = -1;
    config->socket_lock_a_gpio = -1;
    config->socket_lock_b_gpio = -1;
    config->socket_lock_detection_gpio = -1;
    config->rcm_gpio = -1;
    config->rcm_test_gpio = -1;
    config->onewire_gpio = -1;

    for (uint8_t i = 0; i < BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX; i++) {
        config->energy_meter_cur_adc_channel[i] = -1;
        config->energy_meter_vlt_adc_channel[i] = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_AUX_INPUT_COUNT; i++) {
        config->aux_inputs[i].gpio = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_AUX_OUTPUT_COUNT; i++) {
        config->aux_outputs[i].gpio = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_AUX_ANALOG_INPUT_COUNT; i++) {
        config->aux_analog_inputs[i].adc_channel = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_SERIAL_COUNT; i++) {
        config->serials[i].rxd_gpio = -1;
        config->serials[i].txd_gpio = -1;
        config->serials[i].rts_gpio = -1;
    }
}

esp_err_t board_config_parse_file(FILE* src, board_cfg_t* board_cfg)
{
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_mark_t key_mark;

    if (!yaml_parser_initialize(&parser)) {
        ESP_LOGE(TAG, "Cant initialize yaml parser");
        return ESP_ERR_INVALID_STATE;
    }

    yaml_parser_set_input_file(&parser, src);

    empty_config(board_cfg);

    int level = -1;
    uint8_t seq_level = 0;
    key_t key[YAML_MAX_LEVEL];
    int seq_idx[YAML_MAX_LEVEL] = { 0 };
    for (int i = 0; i < YAML_MAX_LEVEL; i++) {
        key[i] = KEY_NONE;
    }
    bool done = false;

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            ESP_LOGE(TAG, "Parsing error: %s (line: %zu column: %zu)", parser.problem, parser.problem_mark.line, parser.problem_mark.column);
            goto error;
        }

        switch (event.type) {
        case YAML_SCALAR_EVENT:
            if (level < YAML_MAX_LEVEL) {
                const char* value = (char*)event.data.scalar.value;
                if (key[level] == KEY_NONE) {
                    key[level] = get_key(value);
                    key_mark = parser.mark;
                    if (key[level] == KEY_NONE) {
                        key[level] = KEY_INVALID;
                        ESP_LOGW(TAG, "Unknow property: %s (line: %zu column: %zu)", value, event.start_mark.line, event.start_mark.column);
                    }
                } else {
                    if (key[level] != KEY_INVALID) {
                        if (!set_key_value(board_cfg, key, seq_idx, value)) {
                            ESP_LOGW(TAG, "Unknow property: %s (line: %zu column: %zu)", keys[key[level]], key_mark.line, key_mark.column);
                        }
                    }
                    if (seq_level & (1 << level)) {
                        seq_idx[level]++;  // for array of primitives
                    } else {
                        key[level] = KEY_NONE;
                    }
                }
            }
            break;
        case YAML_SEQUENCE_START_EVENT:
            if (level < YAML_MAX_LEVEL) {
                seq_level |= (1 << level);
            }
            break;
        case YAML_SEQUENCE_END_EVENT:
            if (level < YAML_MAX_LEVEL) {
                seq_level &= ~(1 << level);
                key[level] = KEY_NONE;  // for array of primitives
                seq_idx[level] = 0;
            }
            break;
        case YAML_MAPPING_START_EVENT:
            level++;
            break;
        case YAML_MAPPING_END_EVENT:
            if (level < YAML_MAX_LEVEL) key[level] = KEY_NONE;
            level--;
            if (level < YAML_MAX_LEVEL) {
                if (seq_level & (1 << level))
                    seq_idx[level]++;  // for array of objects, key[level] clear YAML_SEQUENCE_END_EVENT
                else
                    key[level] = KEY_NONE;
            }
            break;
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        default:
            break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);

    return ESP_OK;

error:
    ESP_LOGE(TAG, "Parsing error");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);

    return ESP_FAIL;
}
