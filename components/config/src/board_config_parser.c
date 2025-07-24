
#include "board_config_parser.h"

#include <esp_log.h>

#include "yaml.h"

#define YAML_MAX_LEVEL 3

#define CASE_SET_VALUE(level, prop, convert_fn) \
    case level:                                 \
        config->prop = convert_fn(value);       \
        break;

#define CASE_SET_VALUE_STR(level, prop, size)   \
    case level:                                 \
        strncpy(config->prop, value, size - 1); \
        break;

#define CASE_SET_SEQ_VALUE(level, prop, convert_fn, condition) \
    case level:                                                \
        if (condition) config->prop = convert_fn(value);       \
        break;

#define CASE_SET_SEQ_VALUE_STR(level, prop, size, condition)   \
    case level:                                                \
        if (condition) strncpy(config->prop, value, size - 1); \
        break;

#define CASE_SET_SEQ_VALUE_STRDUP(level, prop, condition) \
    case level:                                           \
        if (condition) config->prop = strdup(value);      \
        break;

#define CASE_DEFAULT() \
    default:           \
        return false;

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
    KEY_LEDS,
    KEY_CHARGING,
    KEY_ERROR,
    KEY_WIFI,
    KEY_BUTTON,
    KEY_AC_RELAY,
    KEY_PILOT,
    KEY_PROXIMITY,
    KEY_SOCKET_LOCK,
    KEY_RCM,
    KEY_AUX,
    KEY_INPUTS,
    KEY_OUTPUTS,
    KEY_ANALOG_INPUTS,
    KEY_GPIO,
    KEY_GPIOS,
    KEY_NAME,
    KEY_ADC_CHANNEL,
    KEY_ADC_CHANNELS,
    KEY_LEVELS,
    KEY_DETECTION_GPIO,
    KEY_DETECTION_DELAY,
    KEY_MIN_BREAK_TIME,
    KEY_TEST_GPIO,
    KEY_ENERGY_METER,
    KEY_CURRENT,
    KEY_VOLTAGE,
    KEY_SCALE,
    KEY_SERIALS,
    KEY_TYPE,
    KEY_RXD_GPIO,
    KEY_TXD_GPIO,
    KEY_RTS_GPIO,
    KEY_ONEWIRE,
    KEY_TEMPERATURE_SENSOR,
    KEY_OTA,
    KEY_CHANNELS,
    KEY_PATH,
    //
    KEY_MAX,
} key_t;

// string values of key_t, must match key_t order
static const char* keys[] = {
    "deviceName", "leds",        "charging", "error",         "wifi",           "button",       "acRelay",  "pilot",       "proximity",
    "socketLock", "rcm",         "aux",      "inputs",        "outputs",        "analogInputs", "gpio",     "gpios",       "name",
    "adcChannel", "adcChannels", "levels",   "detectionGpio", "detectionDelay", "minBreakTime", "testGpio", "energyMeter", "current",
    "voltage",    "scale",       "serials",  "type",          "rxdGpio",        "txdGpio",      "rtsGpio",  "onewire",     "temperatureSensor",
    "ota",        "channels",    "path",
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
        CASE_SET_VALUE_STR(KEY_DEVICE_NAME, device_name, BOARD_CFG_DEVICE_NAME_SIZE);
    case KEY_LEDS:
        switch (key[1]) {
        case KEY_CHARGING:
            switch (key[2]) {
                CASE_SET_VALUE(KEY_GPIO, leds.charging_gpio, str_to_gpio);
                CASE_DEFAULT();
            }
            break;
        case KEY_ERROR:
            switch (key[2]) {
                CASE_SET_VALUE(KEY_GPIO, leds.error_gpio, str_to_gpio);
                CASE_DEFAULT();
            }
            break;
        case KEY_WIFI:
            switch (key[2]) {
                CASE_SET_VALUE(KEY_GPIO, leds.wifi_gpio, str_to_gpio);
                CASE_DEFAULT();
            }
            break;
        default:
            return false;
        }
        break;
    case KEY_BUTTON:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_GPIO, button.gpio, str_to_gpio);
            CASE_DEFAULT();
        }
        break;
    case KEY_AC_RELAY:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE(KEY_GPIOS, ac_relay.gpios[seq_idx[1]], atoi, seq_idx[1] < BOARD_CFG_AC_RELAY_GPIO_MAX);
            CASE_DEFAULT();
        }
        break;
    case KEY_PILOT:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_GPIO, pilot.gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_ADC_CHANNEL, pilot.adc_channel, str_to_gpio);
            CASE_SET_SEQ_VALUE(KEY_LEVELS, pilot.levels[seq_idx[1]], atoi, seq_idx[1] < BOARD_CFG_PILOT_LEVEL_MAX);
            CASE_DEFAULT();
        }
        break;
    case KEY_PROXIMITY:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_ADC_CHANNEL, proximity.adc_channel, str_to_gpio);
            CASE_SET_SEQ_VALUE(KEY_LEVELS, proximity.levels[seq_idx[1]], atoi, seq_idx[1] < BOARD_CFG_PROXIMITY_LEVEL_MAX);
            CASE_DEFAULT();
        }
        break;
    case KEY_SOCKET_LOCK:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE(KEY_GPIOS, socket_lock.gpios[seq_idx[1]], atoi, seq_idx[1] < BOARD_CFG_SOCKET_LOCK_GPIO_MAX);
            CASE_SET_VALUE(KEY_DETECTION_GPIO, socket_lock.detection_gpio, atoi);
            CASE_SET_VALUE(KEY_DETECTION_DELAY, socket_lock.detection_delay, atoi);
            CASE_SET_VALUE(KEY_MIN_BREAK_TIME, socket_lock.min_break_time, atoi);
            CASE_DEFAULT();
        }
        break;
    case KEY_ENERGY_METER:
        switch (key[1]) {
        case KEY_CURRENT:
            switch (key[2]) {
                CASE_SET_SEQ_VALUE(KEY_ADC_CHANNELS, energy_meter.cur_adc_channel[seq_idx[2]], str_to_gpio, seq_idx[2] < BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX);
                CASE_SET_VALUE(KEY_SCALE, energy_meter.cur_scale, atof);
                CASE_DEFAULT();
            }
            break;
        case KEY_VOLTAGE:
            switch (key[2]) {
                CASE_SET_SEQ_VALUE(KEY_ADC_CHANNELS, energy_meter.vlt_adc_channel[seq_idx[2]], str_to_gpio, seq_idx[2] < BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX);
                CASE_SET_VALUE(KEY_SCALE, energy_meter.vlt_scale, atof);
                CASE_DEFAULT();
            }
            break;
        default:
            return false;
        }
        break;
    case KEY_RCM:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_GPIO, rcm.gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_TEST_GPIO, rcm.test_gpio, str_to_gpio);
            CASE_DEFAULT();
        }
        break;
    case KEY_AUX:
        switch (key[1]) {
        case KEY_INPUTS:
            switch (key[2]) {
                CASE_SET_SEQ_VALUE_STR(KEY_NAME, aux.inputs[seq_idx[1]].name, BOARD_CFG_AUX_NAME_SIZE, seq_idx[1] < BOARD_CFG_AUX_INPUT_COUNT);
                CASE_SET_SEQ_VALUE(KEY_GPIO, aux.inputs[seq_idx[1]].gpio, str_to_gpio, seq_idx[1] < BOARD_CFG_AUX_INPUT_COUNT);
                CASE_DEFAULT();
            }
            break;
        case KEY_OUTPUTS:
            switch (key[2]) {
                CASE_SET_SEQ_VALUE_STR(KEY_NAME, aux.outputs[seq_idx[1]].name, BOARD_CFG_AUX_NAME_SIZE, seq_idx[1] < BOARD_CFG_AUX_OUTPUT_COUNT);
                CASE_SET_SEQ_VALUE(KEY_GPIO, aux.outputs[seq_idx[1]].gpio, str_to_gpio, seq_idx[1] < BOARD_CFG_AUX_OUTPUT_COUNT);
                CASE_DEFAULT();
            }
            break;
        case KEY_ANALOG_INPUTS:
            switch (key[2]) {
                CASE_SET_SEQ_VALUE_STR(KEY_NAME, aux.analog_inputs[seq_idx[1]].name, BOARD_CFG_AUX_NAME_SIZE, seq_idx[1] < BOARD_CFG_AUX_ANALOG_INPUT_COUNT);
                CASE_SET_SEQ_VALUE(KEY_ADC_CHANNEL, aux.analog_inputs[seq_idx[1]].adc_channel, str_to_gpio, seq_idx[1] < BOARD_CFG_AUX_ANALOG_INPUT_COUNT);
                CASE_DEFAULT();
            }
            break;
        default:
            return false;
        }
        break;
    case KEY_SERIALS:
        switch (key[1]) {
            CASE_SET_SEQ_VALUE(KEY_TYPE, serials[seq_idx[0]].type, str_to_serial_type, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE_STR(KEY_NAME, serials[seq_idx[0]].name, BOARD_CFG_SERIAL_NAME_SIZE, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE(KEY_RXD_GPIO, serials[seq_idx[0]].rxd_gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE(KEY_TXD_GPIO, serials[seq_idx[0]].txd_gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_SET_SEQ_VALUE(KEY_RTS_GPIO, serials[seq_idx[0]].rts_gpio, str_to_gpio, seq_idx[0] < BOARD_CFG_SERIAL_COUNT);
            CASE_DEFAULT();
        }
        break;
    case KEY_ONEWIRE:
        switch (key[1]) {
            CASE_SET_VALUE(KEY_GPIO, onewire.gpio, str_to_gpio);
            CASE_SET_VALUE(KEY_TEMPERATURE_SENSOR, onewire.temp_sensor, str_to_bool);
            CASE_DEFAULT();
        }
        break;
    case KEY_OTA:
        switch (key[1]) {
        case KEY_CHANNELS:
            switch (key[2]) {
                CASE_SET_SEQ_VALUE_STR(KEY_NAME, ota.channels[seq_idx[1]].name, BOARD_CFG_SERIAL_NAME_SIZE, seq_idx[1] < BOARD_CFG_OTA_CHANNEL_COUNT);
                CASE_SET_SEQ_VALUE_STRDUP(KEY_PATH, ota.channels[seq_idx[1]].path, seq_idx[1] < BOARD_CFG_OTA_CHANNEL_COUNT);
                CASE_DEFAULT();
            }
            break;
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
    config->leds.charging_gpio = -1;
    config->leds.error_gpio = -1;
    config->leds.wifi_gpio = -1;
    config->button.gpio = -1;
    config->pilot.gpio = -1;
    config->pilot.adc_channel = -1;
    config->proximity.adc_channel = -1;
    config->ac_relay.gpios[BOARD_CFG_AC_RELAY_GPIO_L1] = -1;
    config->ac_relay.gpios[BOARD_CFG_AC_RELAY_GPIO_L2_L3] = -1;
    config->socket_lock.gpios[BOARD_CFG_SOCKET_LOCK_GPIO_A] = -1;
    config->socket_lock.gpios[BOARD_CFG_SOCKET_LOCK_GPIO_B] = -1;
    config->socket_lock.detection_gpio = -1;
    config->rcm.gpio = -1;
    config->rcm.test_gpio = -1;
    config->onewire.gpio = -1;

    for (uint8_t i = 0; i < BOARD_CFG_ENERGY_METER_ADC_CHANNEL_MAX; i++) {
        config->energy_meter.cur_adc_channel[i] = -1;
        config->energy_meter.vlt_adc_channel[i] = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_AUX_INPUT_COUNT; i++) {
        config->aux.inputs[i].gpio = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_AUX_OUTPUT_COUNT; i++) {
        config->aux.outputs[i].gpio = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_AUX_ANALOG_INPUT_COUNT; i++) {
        config->aux.analog_inputs[i].adc_channel = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_SERIAL_COUNT; i++) {
        config->serials[i].rxd_gpio = -1;
        config->serials[i].txd_gpio = -1;
        config->serials[i].rts_gpio = -1;
    }
    for (uint8_t i = 0; i < BOARD_CFG_OTA_CHANNEL_COUNT; i++) {
        config->ota.channels[i].name[0] = '\0';
        config->ota.channels[i].path = NULL;
    }
}

esp_err_t board_config_parse_file(FILE* src, board_cfg_t* board_cfg)
{
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_mark_t key_mark;

    if (!yaml_parser_initialize(&parser)) {
        ESP_LOGE(TAG, "Can initialize yaml parser");
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
