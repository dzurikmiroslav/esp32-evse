#include "serial_nextion.h"

#include <esp_log.h>
#include <string.h>
#include <unistd.h>

#include "nextion_task.h"
#include "serial_mode.h"

#define BUF_SIZE 256

static const char* NEX_CMD_WAKE = "sleep=0";
static const char* NEX_CMD_FULL_BRIGHTNESS = "dims=100";

static const uint8_t DELIMITER[] = { 0xFF, 0xFF, 0xFF };

static int port;

static int fd;

static bool upload_first_chunk;

static void tx_str(const char* cmd)
{
    uart_write_bytes(port, cmd, strlen(cmd));
    uart_write_bytes(port, DELIMITER, 3);
}

esp_err_t serial_nextion_get_info(serial_nextion_info_t* info)
{
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    port = serial_port_find(SERIAL_MODE_NEXTION_NAME);
    if (port != -1 && serial_tasks[port].task) {
        nextion_task_pause(serial_tasks[port].task);

        tx_str("connect");

        char buf[BUF_SIZE];
        int len = uart_read_bytes(port, buf, BUF_SIZE, pdMS_TO_TICKS(500));
        if (len > 0) {
            buf[len] = '\0';
            ret = ESP_ERR_INVALID_STATE;

            int touch;
            char* saveptr;
            char* token = strtok_r(buf, ",", &saveptr);
            for (int i = 0; token; token = strtok_r(NULL, ",", &saveptr), i++) {
                switch (i) {
                case 0:
                    if (sscanf(token, "comok %d", &touch) != EOF) {
                        info->touch = touch > 0;
                        ret = ESP_OK;
                    }
                    break;
                case 2:
                    strcpy(info->model, token);
                    break;
                case 6:
                    sscanf(token, "%" PRIu32, &info->flash_size);
                    break;
                default:
                    break;
                }
            }
        }

        nextion_task_resume(serial_tasks[port].task);
    }

    return ret;
}

static char upload_read_response(void)
{
    for (int i = 0; i < 50; i++) {  // 50 * 100 ms
        char res;
        int len = uart_read_bytes(port, &res, sizeof(char), pdMS_TO_TICKS(100));
        if (len > 0) {
            return res;
        }
    }

    return 0x00;
}

static size_t upload_read_skip_to(void)
{
    for (int i = 0; i < 50; i++) {  // 50 * 100 ms
        char buf[10];
        int len = uart_read_bytes(port, buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (len >= 4) {
            size_t res = 0;
            for (int i = 0; i < 4; ++i) {
                res += (buf[i] << (8 * i));
            }
            return res;
        }
    }

    return 0;
}

esp_err_t serial_nextion_upload_begin(size_t file_size, uint32_t baud_rate)
{
    port = serial_port_find(SERIAL_MODE_NEXTION_NAME);

    if (port == -1) return ESP_ERR_NOT_FOUND;

    fd = nextion_task_get_fd(serial_tasks[port].task);
    nextion_task_stop(serial_tasks[port].task);
    serial_tasks[port].task = NULL;

    tx_str(NEX_CMD_FULL_BRIGHTNESS);
    tx_str(NEX_CMD_WAKE);
    vTaskDelay(pdMS_TO_TICKS(250));
    uart_flush_input(port);

    char buf[BUF_SIZE];
    sprintf(buf, "whmi-wris %" PRIu32 ",%" PRIu32 ",1", (uint32_t)file_size, baud_rate);  // TODO why PRIuSIZE for size_t not work?
    tx_str(buf);

    uint32_t curr_baud_rate;
    uart_get_baudrate(port, &curr_baud_rate);
    if (curr_baud_rate != baud_rate) {
        uart_wait_tx_done(port, pdMS_TO_TICKS(1000));
        uart_set_baudrate(port, baud_rate);
    }

    if (upload_read_response() != 0x05) {
        serial_nextion_upload_end();
        return ESP_ERR_INVALID_STATE;
    }

    upload_first_chunk = true;

    return ESP_OK;
}

esp_err_t serial_nextion_upload_write(char* data, uint16_t len, size_t* skip_to)
{
    if (port == -1) return ESP_ERR_NOT_FOUND;

    uart_write_bytes(port, data, len);

    switch (upload_read_response()) {
    case 0x05:
        *skip_to = 0;
        break;
    case 0x08:
        *skip_to = upload_read_skip_to();
        break;
    default:
        serial_nextion_upload_end();
        return ESP_ERR_INVALID_STATE;
    }

    upload_first_chunk = false;

    return ESP_OK;
}

esp_err_t serial_nextion_upload_end(void)
{
    if (port == -1) return ESP_ERR_NOT_FOUND;

    int baud_rate = serial_get_baud_rate(port);
    uint32_t curr_baud_rate;
    uart_get_baudrate(port, &curr_baud_rate);
    if (curr_baud_rate != baud_rate) {
        uart_wait_tx_done(port, pdMS_TO_TICKS(1000));
        uart_set_baudrate(port, baud_rate);
    }

    serial_tasks[port].task = nextion_task_start(fd);

    return ESP_OK;
}
