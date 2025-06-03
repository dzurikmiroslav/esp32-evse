#include "serial_at.h"

#include "at_cmds.h"
#include "esp_log.h"

#define BUF_SIZE 256

static const char* TAG = "serial_at";

static uart_port_t port = -1;

static TaskHandle_t serial_at_task = NULL;

static int write_char(char ch)
{
    if (ch == 10) {
        uart_write_bytes(port, "\r\n", 2);
    } else {
        uart_write_bytes(port, &ch, 1);
    }

    return 1;
}

static int read_char(char* ch)
{
    int readed = uart_read_bytes(port, ch, 1, 1);

    if (readed > 0) {
        if (*ch == 13) {
            *ch = 10;
        }
        if (at_echo) {
            write_char(*ch);
        }
    }

    return readed;
}

static struct cat_io_interface iface = { .read = read_char, .write = write_char };

static void serial_at_task_func(void* param)
{
    uint8_t buf[BUF_SIZE];

    struct cat_command_group* cmd_desc[] = { &at_cmd_group };

    struct cat_descriptor desc = { .cmd_group = cmd_desc, .cmd_group_num = sizeof(cmd_desc) / sizeof(cmd_desc[0]), .buf = buf, .buf_size = sizeof(buf) };

    struct cat_object at;

    cat_init(&at, &desc, &iface, NULL);

    while (true) {
        while (cat_service(&at) != 0) {
        };

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void serial_at_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485)
{
    ESP_LOGI(TAG, "Starting on uart %d", uart_num);

    uart_config_t uart_config = { .baud_rate = baud_rate,
                                  .data_bits = data_bits,
                                  .parity = parity,
                                  .stop_bits = stop_bit,
                                  .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                                  .rx_flow_ctrl_thresh = 122,
                                  .source_clk = UART_SCLK_APB };

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

    xTaskCreate(serial_at_task_func, "serial_at_task", 2 * 1024, NULL, 5, &serial_at_task);
}

void serial_at_stop(void)
{
    ESP_LOGI(TAG, "Stopping");

    if (serial_at_task) {
        vTaskDelete(serial_at_task);
        serial_at_task = NULL;
    }

    if (port != -1) {
        uart_driver_delete(port);
        port = -1;
    }
}