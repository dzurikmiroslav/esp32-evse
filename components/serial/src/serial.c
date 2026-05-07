#include "serial.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/uart_vfs.h>
#include <esp_log.h>
#include <fcntl.h>
#include <nvs.h>
#include <string.h>
#include <unistd.h>

#include "at_task.h"
#include "board_config.h"
#include "logger_task.h"
#include "modbus_rtu_task.h"
#include "nextion_task.h"
#include "serial_mode.h"
#include "serial_nextion.h"

#define BAUD_RATE_MIN 300
#define BAUD_RATE_MAX 1000000

#define NVS_NAMESPACE "serial"
#define NVS_MODE      "mode_%x"
#define NVS_BAUD_RATE "baud_rate_%x"
#define NVS_DATA_BITS "data_bits_%x"
#define NVS_STOP_BITS "stop_bits_%x"
#define NVS_PARITY    "parity_%x"

static const char* TAG = "serial";

static nvs_handle_t nvs;

serial_mode_conf_t serial_mode_configs[] = {
    {
        .name = SERIAL_MODE_LOG_NAME,
        .start = logger_task_start,
        .stop = logger_task_stop,
        .rx_buffer_size = LOGGER_TASK_RX_BUFFER_SIZE,
        .tx_buffer_size = LOGGER_TASK_TX_BUFFER_SIZE,
    },
    {
        .name = SERIAL_MODE_AT_NAME,
        .start = at_task_start,
        .stop = at_task_stop,
        .rx_buffer_size = AT_TASK_RX_BUFFER_SIZE,
        .tx_buffer_size = AT_TASK_TX_BUFFER_SIZE,
    },
    {
        .name = SERIAL_MODE_NEXTION_NAME,
        .start = nextion_task_start,
        .stop = nextion_task_stop,
        .rx_buffer_size = NEXTION_TASK_RX_BUFFER_SIZE,
        .tx_buffer_size = NEXTION_TASK_TX_BUFFER_SIZE,
    },
    {
        .name = SERIAL_MODE_MODBUS_NAME,
        .start = modbus_rtu_task_start,
        .stop = modbus_rtu_task_stop,
        .rx_buffer_size = MODBUS_RTU_TASK_RX_BUFFER_SIZE,
        .tx_buffer_size = MODBUS_RTU_TASK_TX_BUFFER_SIZE,
    },
    {
        .name = SERIAL_MODE_SCRIPT_NAME,
        .start = NULL,
        .stop = NULL,
        .rx_buffer_size = 256,
        .tx_buffer_size = 0,
    },
};

serial_task_t serial_tasks[SERIAL_ID_MAX];

int serial_port_find(const char* mode_name)
{
    for (int i = 0; i < SERIAL_ID_MAX; i++) {
        if (serial_tasks[i].mode && strcmp(mode_name, serial_tasks[i].mode->name) == 0) return i;
    }

    return -1;
}

int serial_fd_find(const char* mode_name)
{
    for (int i = 0; i < SERIAL_ID_MAX; i++) {
        if (serial_tasks[i].mode && strcmp(mode_name, serial_tasks[i].mode->name) == 0) return serial_tasks[i].fd;
    }

    return -1;
}

static serial_mode_conf_t* mode_config_find(const char* name)
{
    if (!name) return NULL;

    for (int i = 0; i < sizeof(serial_mode_configs) / sizeof(serial_mode_conf_t); i++) {
        if (strcmp(name, serial_mode_configs[i].name) == 0) return &serial_mode_configs[i];
    }

    return NULL;
}

static esp_err_t serial_start(serial_id_t id, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bits, uart_parity_t parity)
{
    esp_err_t err;
    // from ESP-IDF 5.5 need set to uart_set_pin after all uart_driver_delete
    if (board_config.serials[id].type == BOARD_CFG_SERIAL_TYPE_UART) {
        err = uart_set_pin(id, board_config.serials[id].txd_gpio, board_config.serials[id].rxd_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    } else {
        err = uart_set_pin(id, board_config.serials[id].txd_gpio, board_config.serials[id].rxd_gpio, board_config.serials[id].rts_gpio, UART_PIN_NO_CHANGE);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin() returned 0x%x", err);
        return err;
    }

    if (serial_tasks[id].mode) {
        uart_config_t uart_config = {
            .baud_rate = baud_rate,
            .data_bits = data_bits,
            .parity = parity,
            .stop_bits = stop_bits,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
            .source_clk = UART_SCLK_DEFAULT,
        };

        err = uart_param_config(id, &uart_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_param_config() returned 0x%x", err);
            return err;
        }

        err = uart_driver_install(id, serial_tasks[id].mode->rx_buffer_size, serial_tasks[id].mode->tx_buffer_size, 0, NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_driver_install() returned 0x%x", err);
            return err;
        }

        if (board_config.serials[id].type == BOARD_CFG_SERIAL_TYPE_RS485) {
            err = uart_set_mode(id, UART_MODE_RS485_HALF_DUPLEX);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "uart_set_mode() returned 0x%x", err);
                return err;
            }
            err = uart_set_rx_timeout(id, 3);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "uart_set_rx_timeout() returned 0x%x", err);
                return err;
            }
        }

        char file_name[16];
        sprintf(file_name, "/dev/uart/%d", id);

        serial_tasks[id].fd = open(file_name, O_RDWR | O_NONBLOCK);
        if (serial_tasks[id].fd == -1) {
            ESP_LOGE(TAG, "Cant open file %s", file_name);
            return ESP_ERR_NOT_FOUND;
        }

        if (serial_tasks[id].mode->start) {
            serial_tasks[id].task = serial_tasks[id].mode->start(serial_tasks[id].fd);
        } else {
            serial_tasks[id].task = NULL;
        }
    }

    return ESP_OK;
}

static void serial_stop(serial_id_t id)
{
    if (serial_tasks[id].mode) {
        if (serial_tasks[id].task) {
            serial_tasks[id].mode->stop(serial_tasks[id].task);
            serial_tasks[id].task = NULL;
        }

        if (serial_tasks[id].fd != -1) {
            close(serial_tasks[id].fd);
            serial_tasks[id].fd = -1;
        }

        esp_err_t err = uart_driver_delete(id);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_driver_delete() returned 0x%x", err);
        }

        // make sure the gpio are in virgin state, before any driver installation
        if (GPIO_IS_VALID_GPIO(board_config.serials[id].rxd_gpio)) gpio_reset_pin(board_config.serials[id].rxd_gpio);
        if (GPIO_IS_VALID_GPIO(board_config.serials[id].txd_gpio)) gpio_reset_pin(board_config.serials[id].txd_gpio);
        if (GPIO_IS_VALID_GPIO(board_config.serials[id].rts_gpio)) gpio_reset_pin(board_config.serials[id].rts_gpio);
    }
}

void serial_init(void)
{
    uart_vfs_dev_register();

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    for (serial_id_t id = SERIAL_ID_1; id < SERIAL_ID_MAX; id++) {
        if (board_config.serials[id].type != BOARD_CFG_SERIAL_TYPE_NONE) {
            //  uart_vfs_dev_use_nonblocking(id);
            uart_vfs_dev_use_driver(id);
            uart_vfs_dev_port_set_rx_line_endings(id, ESP_LINE_ENDINGS_LF);
            uart_vfs_dev_port_set_tx_line_endings(id, ESP_LINE_ENDINGS_LF);

            char key[12];
            sprintf(key, NVS_MODE, id);

            size_t mode_name_len = SEIAL_MODE_NAME_SIZE;
            char mode_name[SEIAL_MODE_NAME_SIZE];
            mode_name[0] = '\0';

            nvs_get_str(nvs, key, mode_name, &mode_name_len);
            serial_tasks[id].mode = mode_config_find(mode_name);

            int baud_rate = serial_get_baud_rate(id);
            uart_word_length_t data_bits = serial_get_data_bits(id);
            uart_stop_bits_t stop_bits = serial_get_stop_bits(id);
            uart_parity_t parity = serial_get_parity(id);

            ESP_ERROR_CHECK(serial_start(id, baud_rate, data_bits, stop_bits, parity));
        }
    }
}

const char* serial_get_mode(serial_id_t id)
{
    return serial_tasks[id].mode ? serial_tasks[id].mode->name : "none";
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

esp_err_t serial_set_config(serial_id_t id, const char* mode, int baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bits, uart_parity_t parity)
{
    if (id < 0 || id >= SERIAL_ID_MAX) {
        ESP_LOGE(TAG, "Serial id out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (board_config.serials[id].type == BOARD_CFG_SERIAL_TYPE_NONE) {
        ESP_LOGE(TAG, "Serial not available");
        return ESP_ERR_NOT_SUPPORTED;
    }

    serial_mode_conf_t* mode_config = mode_config_find(mode);

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
    nvs_set_str(nvs, key, mode_config ? mode_config->name : "none");

    sprintf(key, NVS_BAUD_RATE, id);
    nvs_set_i32(nvs, key, baud_rate);

    sprintf(key, NVS_DATA_BITS, id);
    nvs_set_u8(nvs, key, data_bits);

    sprintf(key, NVS_STOP_BITS, id);
    nvs_set_u8(nvs, key, stop_bits);

    sprintf(key, NVS_PARITY, id);
    nvs_set_u8(nvs, key, parity);

    nvs_commit(nvs);

    serial_stop(id);

    serial_tasks[id].mode = mode_config;

    return serial_start(id, baud_rate, data_bits, stop_bits, parity);
}

const char* serial_data_bits_to_str(uart_word_length_t bits)
{
    switch (bits) {
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
    switch (bits) {
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
    switch (parity) {
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
