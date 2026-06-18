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

serial_task_t serial_tasks[UART_NUM_MAX];

uart_port_t serial_port_find(const char* mode_name)
{
    for (uart_port_t port = UART_NUM_0; port < UART_NUM_MAX; port++) {
        if (serial_tasks[port].mode && strcmp(mode_name, serial_tasks[port].mode->name) == 0) return port;
    }

    return -1;
}

int serial_fd_find(const char* mode_name)
{
    for (uart_port_t port = UART_NUM_0; port < UART_NUM_MAX; port++) {
        if (serial_tasks[port].mode && strcmp(mode_name, serial_tasks[port].mode->name) == 0) return serial_tasks[port].fd;
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

static esp_err_t serial_start(uart_port_t port, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bits, uart_parity_t parity)
{
    esp_err_t err;
    // from ESP-IDF 5.5 need set to uart_set_pin after all uart_driver_delete
    if (board_config.serials[port].type == BOARD_CFG_SERIAL_TYPE_UART) {
        err = uart_set_pin(port, board_config.serials[port].txd_gpio, board_config.serials[port].rxd_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    } else {
        err = uart_set_pin(port, board_config.serials[port].txd_gpio, board_config.serials[port].rxd_gpio, board_config.serials[port].rts_gpio, UART_PIN_NO_CHANGE);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin() returned 0x%x", err);
        return err;
    }

    if (serial_tasks[port].mode) {
        uart_config_t uart_config = {
            .baud_rate = baud_rate,
            .data_bits = data_bits,
            .parity = parity,
            .stop_bits = stop_bits,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
            .source_clk = UART_SCLK_DEFAULT,
        };

        err = uart_param_config(port, &uart_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_param_config() returned 0x%x", err);
            return err;
        }

        err = uart_driver_install(port, serial_tasks[port].mode->rx_buffer_size, serial_tasks[port].mode->tx_buffer_size, 0, NULL, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_driver_install() returned 0x%x", err);
            return err;
        }

        if (board_config.serials[port].type == BOARD_CFG_SERIAL_TYPE_RS485) {
            err = uart_set_mode(port, UART_MODE_RS485_HALF_DUPLEX);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "uart_set_mode() returned 0x%x", err);
                return err;
            }
            err = uart_set_rx_timeout(port, 3);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "uart_set_rx_timeout() returned 0x%x", err);
                return err;
            }
        }

        char file_name[16];
        sprintf(file_name, "/dev/uart/%d", port);

        serial_tasks[port].fd = open(file_name, O_RDWR | O_NONBLOCK);
        if (serial_tasks[port].fd == -1) {
            ESP_LOGE(TAG, "Cant open file %s", file_name);
            return ESP_ERR_NOT_FOUND;
        }

        if (serial_tasks[port].mode->start) {
            serial_tasks[port].task = serial_tasks[port].mode->start(serial_tasks[port].fd);
        } else {
            serial_tasks[port].task = NULL;
        }
    }

    return ESP_OK;
}

static void serial_stop(uart_port_t port)
{
    if (serial_tasks[port].mode) {
        if (serial_tasks[port].task) {
            serial_tasks[port].mode->stop(serial_tasks[port].task);
            serial_tasks[port].task = NULL;
        }

        if (serial_tasks[port].fd != -1) {
            close(serial_tasks[port].fd);
            serial_tasks[port].fd = -1;
        }

        esp_err_t err = uart_driver_delete(port);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_driver_delete() returned 0x%x", err);
        }

        // make sure the gpio are in virgin state, before any driver installation
        if (GPIO_IS_VALID_GPIO(board_config.serials[port].rxd_gpio)) gpio_reset_pin(board_config.serials[port].rxd_gpio);
        if (GPIO_IS_VALID_GPIO(board_config.serials[port].txd_gpio)) gpio_reset_pin(board_config.serials[port].txd_gpio);
        if (GPIO_IS_VALID_GPIO(board_config.serials[port].rts_gpio)) gpio_reset_pin(board_config.serials[port].rts_gpio);
    }
}

void serial_init(void)
{
    uart_vfs_dev_register();

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    for (uart_port_t port = UART_NUM_0; port < UART_NUM_MAX; port++) {
        if (board_config.serials[port].type != BOARD_CFG_SERIAL_TYPE_NONE) {
            //  uart_vfs_dev_use_nonblocking(port);
            uart_vfs_dev_use_driver(port);
            uart_vfs_dev_port_set_rx_line_endings(port, ESP_LINE_ENDINGS_LF);
            uart_vfs_dev_port_set_tx_line_endings(port, ESP_LINE_ENDINGS_LF);

            char key[12];
            sprintf(key, NVS_MODE, port);

            size_t mode_name_len = SEIAL_MODE_NAME_SIZE;
            char mode_name[SEIAL_MODE_NAME_SIZE];
            if (port == UART_NUM_0)
                strlcpy(mode_name, SERIAL_MODE_LOG_NAME, sizeof(mode_name));  // UART_NUM_0 is default system log
            else
                mode_name[0] = '\0';
            nvs_get_str(nvs, key, mode_name, &mode_name_len);
            serial_tasks[port].mode = mode_config_find(mode_name);

            int baud_rate = serial_get_baud_rate(port);
            uart_word_length_t data_bits = serial_get_data_bits(port);
            uart_stop_bits_t stop_bits = serial_get_stop_bits(port);
            uart_parity_t parity = serial_get_parity(port);

            ESP_ERROR_CHECK(serial_start(port, baud_rate, data_bits, stop_bits, parity));
        }
    }
}

const char* serial_get_mode(uart_port_t port)
{
    return serial_tasks[port].mode ? serial_tasks[port].mode->name : "none";
}

int serial_get_baud_rate(uart_port_t port)
{
    int32_t value = 115200;
    char key[12];
    sprintf(key, NVS_BAUD_RATE, port);
    nvs_get_i32(nvs, key, &value);
    return value;
}

uart_word_length_t serial_get_data_bits(uart_port_t port)
{
    uint8_t value = UART_DATA_8_BITS;
    char key[12];
    sprintf(key, NVS_DATA_BITS, port);
    nvs_get_u8(nvs, key, &value);
    return value;
}

uart_stop_bits_t serial_get_stop_bits(uart_port_t port)
{
    uint8_t value = UART_STOP_BITS_1;
    char key[12];
    sprintf(key, NVS_STOP_BITS, port);
    nvs_get_u8(nvs, key, &value);
    return value;
}

uart_parity_t serial_get_parity(uart_port_t port)
{
    uint8_t value = UART_PARITY_DISABLE;
    char key[12];
    sprintf(key, NVS_PARITY, port);
    nvs_get_u8(nvs, key, &value);
    return value;
}

esp_err_t serial_set_config(uart_port_t port, const char* mode, int baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bits, uart_parity_t parity)
{
    if (port < UART_NUM_0 || port >= UART_NUM_MAX) {
        ESP_LOGE(TAG, "Serial out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (board_config.serials[port].type == BOARD_CFG_SERIAL_TYPE_NONE) {
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
        ESP_LOGE(TAG, "Parity invalport value");
        return ESP_ERR_INVALID_ARG;
    }

    char key[12];
    sprintf(key, NVS_MODE, port);
    nvs_set_str(nvs, key, mode_config ? mode_config->name : "none");

    sprintf(key, NVS_BAUD_RATE, port);
    nvs_set_i32(nvs, key, baud_rate);

    sprintf(key, NVS_DATA_BITS, port);
    nvs_set_u8(nvs, key, data_bits);

    sprintf(key, NVS_STOP_BITS, port);
    nvs_set_u8(nvs, key, stop_bits);

    sprintf(key, NVS_PARITY, port);
    nvs_set_u8(nvs, key, parity);

    nvs_commit(nvs);

    serial_stop(port);

    serial_tasks[port].mode = mode_config;

    return serial_start(port, baud_rate, data_bits, stop_bits, parity);
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
