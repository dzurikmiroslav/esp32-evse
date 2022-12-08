#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs.h"

#include "board_config.h"
#include "serial.h"
#include "serial_logger.h"
#include "serial_at.h"
#include "serial_modbus.h"

#define BAUD_RATE_MIN           2400
#define BAUD_RATE_MAX           230400

#define NVS_NAMESPACE           "serial"
#define NVS_MODE                "mode_%x"
#define NVS_BAUD_RATE           "baud_rate_%x"
#define NVS_DATA_BITS           "data_bits_%x"
#define NVS_STOP_BITS           "stop_bits_%x"
#define NVS_PARITY              "parity_%x"

static const char* TAG = "serial";

static nvs_handle_t nvs;

static serial_mode_t modes[SERIAL_ID_MAX];

static board_config_serial_t serial_board_config[SERIAL_ID_MAX];

void serial_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    serial_board_config[SERIAL_ID_1] = board_config.serial_1;
    if (board_config.serial_1 == BOARD_CONFIG_SERIAL_UART) {
        ESP_ERROR_CHECK(uart_set_pin(SERIAL_ID_1, board_config.serial_1_txd_gpio, board_config.serial_1_rxd_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }
    if (board_config.serial_1 == BOARD_CONFIG_SERIAL_RS485) {
        ESP_ERROR_CHECK(uart_set_pin(SERIAL_ID_1, board_config.serial_1_txd_gpio, board_config.serial_1_rxd_gpio, board_config.serial_1_rts_gpio, UART_PIN_NO_CHANGE));
    }

    serial_board_config[SERIAL_ID_2] = board_config.serial_2;
    if (board_config.serial_2 == BOARD_CONFIG_SERIAL_UART) {
        ESP_ERROR_CHECK(uart_set_pin(SERIAL_ID_2, board_config.serial_2_txd_gpio, board_config.serial_2_rxd_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }
    if (board_config.serial_2 == BOARD_CONFIG_SERIAL_RS485) {
        ESP_ERROR_CHECK(uart_set_pin(SERIAL_ID_2, board_config.serial_2_txd_gpio, board_config.serial_2_rxd_gpio, board_config.serial_2_rts_gpio, UART_PIN_NO_CHANGE));
    }

#if SOC_UART_NUM > 2
    serial_board_config[SERIAL_ID_3] = board_config.serial_3;
    if (board_config.serial_3 == BOARD_CONFIG_SERIAL_UART) {
        ESP_ERROR_CHECK(uart_set_pin(SERIAL_ID_3, board_config.serial_3_txd_gpio, board_config.serial_3_rxd_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }
    if (board_config.serial_3 == BOARD_CONFIG_SERIAL_RS485) {
        ESP_ERROR_CHECK(uart_set_pin(SERIAL_ID_3, board_config.serial_3_txd_gpio, board_config.serial_3_rxd_gpio, board_config.serial_3_rts_gpio, UART_PIN_NO_CHANGE));
    }
#endif

    for (serial_id_t id = 0; id < SERIAL_ID_MAX; id++) {
        if (serial_board_config[id] != BOARD_CONFIG_SERIAL_NONE) {
            uint8_t u8 = 0;
            char key[12];
            sprintf(key, NVS_MODE, id);
            nvs_get_u8(nvs, key, &u8);
            modes[id] = u8;

            int baud_rate = serial_get_baud_rate(id);
            uart_word_length_t data_bits = serial_get_data_bits(id);
            uart_stop_bits_t stop_bits = serial_get_stop_bits(id);
            uart_parity_t parity = serial_get_parity(id);

            switch (modes[id]) {
            case SERIAL_MODE_LOG:
                serial_logger_start(id, baud_rate, data_bits, stop_bits, parity, serial_board_config[id] == BOARD_CONFIG_SERIAL_RS485);
                break;
            case SERIAL_MODE_AT:
                serial_at_start(id, baud_rate, data_bits, stop_bits, parity, serial_board_config[id] == BOARD_CONFIG_SERIAL_RS485);
                break;
            case SERIAL_MODE_MODBUS:
                serial_modbus_start(id, baud_rate, data_bits, stop_bits, parity, serial_board_config[id] == BOARD_CONFIG_SERIAL_RS485);
                break;
            default:
                break;
            }
        }
    }
}

bool serial_is_available(serial_id_t id)
{
    if (id < 0 || id >= SERIAL_ID_MAX) {
        return false;
    }

    return serial_board_config[id] != BOARD_CONFIG_SERIAL_NONE;
}

serial_mode_t serial_get_mode(serial_id_t id)
{
    return modes[id];
}

int serial_get_baud_rate(serial_id_t id)
{
    int32_t value = 115200;
    char key[12];
    sprintf(key, NVS_BAUD_RATE, id);
    nvs_get_i32(nvs, key, &value);
    return value;
}

uart_word_length_t serial_get_data_bits(serial_id_t id)
{
    uint8_t value = UART_DATA_8_BITS;
    char key[12];
    sprintf(key, NVS_DATA_BITS, id);
    nvs_get_u8(nvs, key, &value);
    return value;
}

uart_stop_bits_t serial_get_stop_bits(serial_id_t id)
{
    uint8_t value = UART_STOP_BITS_1;
    char key[12];
    sprintf(key, NVS_STOP_BITS, id);
    nvs_get_u8(nvs, key, &value);
    return value;
}

uart_parity_t serial_get_parity(serial_id_t id)
{
    uint8_t value = UART_PARITY_DISABLE;
    char key[12];
    sprintf(key, NVS_PARITY, id);
    nvs_get_u8(nvs, key, &value);
    return value;
}

void serial_reset_config(void)
{
    for (serial_id_t i = 0; i < SERIAL_ID_MAX; i++) {
        switch (modes[i]) {
        case SERIAL_MODE_LOG:
            serial_logger_stop();
            break;
        case SERIAL_MODE_AT:
            serial_at_stop();
            break;
        case SERIAL_MODE_MODBUS:
            serial_modbus_stop();
            break;
        default:
            break;
        }

        modes[i] = SERIAL_MODE_NONE;
        
        char key[12];
        sprintf(key, NVS_MODE, i);
        nvs_set_u8(nvs, key,  modes[i]);
    }

    nvs_commit(nvs);
}

esp_err_t serial_set_config(serial_id_t id, serial_mode_t mode, int baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bits, uart_parity_t parity)
{
    if (id < 0 || id >= SERIAL_ID_MAX) {
        ESP_LOGE(TAG, "Serial id out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (serial_board_config[id] == BOARD_CONFIG_SERIAL_NONE) {
        ESP_LOGE(TAG, "Serial not available");
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (mode < 0 || mode >= SERIAL_MODE_MAX) {
        ESP_LOGE(TAG, "Mode out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (mode != SERIAL_MODE_NONE) {
        for (serial_id_t i = 0; i < SERIAL_ID_MAX; i++) {
            if (i != id && modes[i] == mode) {
                ESP_LOGE(TAG, "Mode already used on other serial");
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    if (baud_rate < BAUD_RATE_MIN || baud_rate > BAUD_RATE_MAX) {
        ESP_LOGE(TAG, "Baud rate out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (data_bits < UART_DATA_5_BITS || data_bits > UART_DATA_8_BITS) {
        ESP_LOGE(TAG, "Data bits out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (stop_bits < UART_STOP_BITS_1 || stop_bits > UART_STOP_BITS_2) {
        ESP_LOGE(TAG, "Stop bits out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (parity != UART_PARITY_DISABLE && parity != UART_PARITY_ODD && parity != UART_PARITY_EVEN) {
        ESP_LOGE(TAG, "Parity invalid value");
        return ESP_ERR_INVALID_ARG;
    }

    char key[12];
    sprintf(key, NVS_MODE, id);
    nvs_set_u8(nvs, key, mode);

    sprintf(key, NVS_BAUD_RATE, id);
    nvs_set_i32(nvs, key, baud_rate);

    sprintf(key, NVS_DATA_BITS, id);
    nvs_set_u8(nvs, key, data_bits);

    sprintf(key, NVS_STOP_BITS, id);
    nvs_set_u8(nvs, key, stop_bits);

    sprintf(key, NVS_PARITY, id);
    nvs_set_u8(nvs, key, parity);

    nvs_commit(nvs);

    switch (modes[id]) {
    case SERIAL_MODE_LOG:
        serial_logger_stop();
        break;
    case SERIAL_MODE_AT:
        serial_at_stop();
        break;
    case SERIAL_MODE_MODBUS:
        serial_modbus_stop();
        break;
    default:
        break;
    }

    modes[id] = mode;

    switch (modes[id]) {
    case SERIAL_MODE_LOG:
        serial_logger_start(id, baud_rate, data_bits, stop_bits, parity, serial_board_config[id] == BOARD_CONFIG_SERIAL_RS485);
        break;
    case SERIAL_MODE_AT:
        serial_at_start(id, baud_rate, data_bits, stop_bits, parity, serial_board_config[id] == BOARD_CONFIG_SERIAL_RS485);
        break;
    case SERIAL_MODE_MODBUS:
        serial_modbus_start(id, baud_rate, data_bits, stop_bits, parity, serial_board_config[id] == BOARD_CONFIG_SERIAL_RS485);
        break;
    default:
        break;
    }

    return ESP_OK;
}

const char* serial_mode_to_str(serial_mode_t mode)
{
    switch (mode)
    {
    case SERIAL_MODE_AT:
        return "at";
    case SERIAL_MODE_LOG:
        return "log";
    case SERIAL_MODE_MODBUS:
        return "modbus";
    default:
        return "none";
    }
}

serial_mode_t serial_str_to_mode(const char* str)
{
    if (!strcmp(str, "at")) {
        return SERIAL_MODE_AT;
    }
    if (!strcmp(str, "log")) {
        return SERIAL_MODE_LOG;
    }
    if (!strcmp(str, "modbus")) {
        return SERIAL_MODE_MODBUS;
    }
    return SERIAL_MODE_NONE;
}

const char* serial_data_bits_to_str(uart_word_length_t bits)
{
    switch (bits)
    {
    case UART_DATA_5_BITS:
        return "5";
    case UART_DATA_6_BITS:
        return "6";
    case UART_DATA_7_BITS:
        return "7";
    case UART_DATA_8_BITS:
        return "8";
    default:
        return "";
    }
}

uart_word_length_t serial_str_to_data_bits(const char* str)
{
    if (!strcmp(str, "5")) {
        return UART_DATA_5_BITS;
    }
    if (!strcmp(str, "6")) {
        return UART_DATA_6_BITS;
    }
    if (!strcmp(str, "7")) {
        return UART_DATA_7_BITS;
    }
    if (!strcmp(str, "8")) {
        return UART_DATA_8_BITS;
    }
    return 0;
}

const char* serial_stop_bits_to_str(uart_stop_bits_t bits)
{
    switch (bits)
    {
    case UART_STOP_BITS_1:
        return "1";
    case UART_STOP_BITS_1_5:
        return "1.5";
    case UART_STOP_BITS_2:
        return "2";
    default:
        return "";
    }
}

uart_stop_bits_t serial_str_to_stop_bits(const char* str)
{
    if (!strcmp(str, "1")) {
        return UART_STOP_BITS_1;
    }
    if (!strcmp(str, "1.5")) {
        return UART_STOP_BITS_1_5;
    }
    if (!strcmp(str, "2")) {
        return UART_STOP_BITS_2;
    }
    return 0;
}

const char* serial_parity_to_str(uart_parity_t parity)
{
    switch (parity)
    {
    case UART_PARITY_DISABLE:
        return "disable";
    case UART_PARITY_EVEN:
        return "even";
    case UART_PARITY_ODD:
        return "odd";
    default:
        return "";
    }
}

uart_parity_t serial_str_to_parity(const char* str)
{
    if (!strcmp(str, "even")) {
        return UART_PARITY_EVEN;
    }
    if (!strcmp(str, "odd")) {
        return UART_PARITY_ODD;
    }
    return UART_PARITY_DISABLE;
}
