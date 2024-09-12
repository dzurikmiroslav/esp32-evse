#include "serial_logger.h"

#include <esp_log.h>

#include "logger.h"

#define BUF_SIZE 256

static const char* TAG = "serial_logger";

static uart_port_t port = -1;

static TaskHandle_t serial_logger_task = NULL;

static void serial_logger_task_func(void* param)
{
    char* str;
    uint16_t str_len;
    uint16_t index = 0;
    while (true) {
        if (xEventGroupWaitBits(logger_event_group, LOGGER_SERIAL_BIT, pdTRUE, pdFALSE, portMAX_DELAY)) {
            while (logger_read(&index, &str, &str_len)) {
                uart_write_bytes(port, str, str_len);
            }
        }
    }
}

void serial_logger_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485)
{
    ESP_LOGI(TAG, "Starting on uart %d", uart_num);

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bit,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config() returned 0x%x", err);
        return;
    }

    err = uart_driver_install(uart_num, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
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

    xTaskCreate(serial_logger_task_func, "serial_logger_task", 2 * 1024, NULL, 5, &serial_logger_task);
}

void serial_logger_stop(void)
{
    ESP_LOGI(TAG, "Stopping");

    if (serial_logger_task) {
        vTaskDelete(serial_logger_task);
        serial_logger_task = NULL;
    }

    if (port != -1) {
        uart_driver_delete(port);
        port = -1;
    }
}