#include "serial_script.h"

#include <esp_log.h>

#define BUF_SIZE         256
#define EVENT_QUEUE_SIZE 20
#define VFS_PATH         "/dev/serial"

static const char* TAG = "serial_script";

static uart_port_t port = -1;

void serial_script_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485)
{
    ESP_LOGI(TAG, "Starting on uart %d", uart_num);

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bit,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config() returned 0x%x", err);
        return;
    }

    err = uart_driver_install(uart_num, BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install() returned 0x%x", err);
        return;
    }

    port = uart_num;

    if (rs485) {
        err = uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_set_mode() returned 0x%x", err);
            return;
        }
        err = uart_set_rx_timeout(uart_num, 3);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_set_rx_timeout() returned 0x%x", err);
            return;
        }
    }

    uart_set_always_rx_timeout(uart_num, true);
}

void serial_script_stop(void)
{
    ESP_LOGI(TAG, "Stopping");

    if (port != -1) {
        esp_err_t err = uart_driver_delete(port);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_driver_delete() returned 0x%x", err);
        }
        port = -1;
    }
}

bool serial_script_is_available(void)
{
    return port != -1;
}

esp_err_t serial_script_write(const char* buf, size_t len)
{
    if (port == -1) {
        ESP_LOGW(TAG, "No script serial available");
        return ESP_ERR_NOT_FOUND;
    }

    uart_write_bytes(port, buf, len);

    return ESP_OK;
}

esp_err_t serial_script_flush(void)
{
    if (port == -1) {
        ESP_LOGW(TAG, "No script serial available");
        return ESP_ERR_NOT_FOUND;
    }

    uart_flush(port);

    return ESP_OK;
}

esp_err_t serial_script_read(char* buf, size_t* len)
{
    if (port == -1) {
        ESP_LOGW(TAG, "No script serial available");
        return ESP_ERR_NOT_FOUND;
    }

    *len = uart_read_bytes(port, buf, *len, 0);

    return ESP_OK;
}