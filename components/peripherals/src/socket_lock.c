#include "socket_lock.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <nvs.h>
#include <string.h>

#include "board_config.h"

#define NVS_NAMESPACE      "socket_lock"
#define NVS_OPERATING_TIME "op_time"
#define NVS_BREAK_TIME     "break_time"
#define NVS_RETRY_COUNT    "retry_count"
#define NVS_DETECTION_HIGH "detect_hi"

#define OPERATING_TIME_MIN 100
#define OPERATING_TIME_MAX 1000
#define LOCK_DELAY         500

#define LOCK_BIT          BIT0
#define UNLOCK_BIT        BIT1
#define REPEAT_LOCK_BIT   BIT2
#define REPEAT_UNLOCK_BIT BIT3

static const char* TAG = "socket_lock";

static nvs_handle_t nvs;

static uint16_t operating_time = 300;

static uint16_t break_time = 1000;

static bool detection_high;

static uint8_t retry_count = 5;

static socket_lock_status_t status;

static TaskHandle_t socket_lock_task;

static bool is_locked(void)
{
    gpio_set_level(board_config.socket_lock_a_gpio, 1);
    gpio_set_level(board_config.socket_lock_b_gpio, 1);

    vTaskDelay(pdMS_TO_TICKS(board_config.socket_lock_detection_delay));

    return gpio_get_level(board_config.socket_lock_detection_gpio) == detection_high;
}

static void socket_lock_task_func(void* param)
{
    uint32_t notification;

    TickType_t previous_tick = 0;
    uint8_t attempt = 0;

    while (true) {
        if (xTaskNotifyWait(0x00, 0xff, &notification, portMAX_DELAY)) {
            if (notification & (LOCK_BIT | UNLOCK_BIT)) {
                attempt = retry_count;
            }

            if (notification & (UNLOCK_BIT | REPEAT_UNLOCK_BIT)) {
                gpio_set_level(board_config.socket_lock_a_gpio, 0);
                gpio_set_level(board_config.socket_lock_b_gpio, 1);
                vTaskDelay(pdMS_TO_TICKS(operating_time));

                if (!is_locked()) {
                    ESP_LOGI(TAG, "Unlock OK");
                    status = SOCKET_LOCK_STATUS_IDLE;
                } else {
                    if (attempt > 1) {
                        ESP_LOGW(TAG, "Not unlocked yet, repeating...");
                        attempt--;
                        xTaskNotify(socket_lock_task, REPEAT_UNLOCK_BIT, eSetBits);
                    } else {
                        ESP_LOGE(TAG, "Not unlocked");
                        status = SOCKET_LOCK_STATUS_UNLOCKING_FAIL;
                    }
                }

                gpio_set_level(board_config.socket_lock_a_gpio, 0);
                gpio_set_level(board_config.socket_lock_b_gpio, 0);
            } else if (notification & (LOCK_BIT | REPEAT_LOCK_BIT)) {
                if (notification & LOCK_BIT) {
                    vTaskDelay(pdMS_TO_TICKS(LOCK_DELAY));  // delay before first lock attempt
                }
                gpio_set_level(board_config.socket_lock_a_gpio, 1);
                gpio_set_level(board_config.socket_lock_b_gpio, 0);
                vTaskDelay(pdMS_TO_TICKS(operating_time));

                if (is_locked()) {
                    ESP_LOGI(TAG, "Lock OK");
                    status = SOCKET_LOCK_STATUS_IDLE;
                } else {
                    if (attempt > 1) {
                        ESP_LOGW(TAG, "Not locked yet, repeating...");
                        attempt--;
                        xTaskNotify(socket_lock_task, REPEAT_LOCK_BIT, eSetBits);
                    } else {
                        ESP_LOGE(TAG, "Not locked");
                        status = SOCKET_LOCK_STATUS_LOCKING_FAIL;
                    }
                }

                gpio_set_level(board_config.socket_lock_a_gpio, 0);
                gpio_set_level(board_config.socket_lock_b_gpio, 0);
            }

            TickType_t delay_tick = xTaskGetTickCount() - previous_tick;
            if (delay_tick < pdMS_TO_TICKS(break_time)) {
                vTaskDelay(pdMS_TO_TICKS(break_time) - delay_tick);
            }
            previous_tick = xTaskGetTickCount();
        }
    }
}

void socket_lock_init(void)
{
    if (board_config.socket_lock) {
        ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

        nvs_get_u16(nvs, NVS_OPERATING_TIME, &operating_time);

        nvs_get_u16(nvs, NVS_BREAK_TIME, &break_time);

        nvs_get_u8(nvs, NVS_RETRY_COUNT, &retry_count);

        uint8_t u8;
        if (nvs_get_u8(nvs, NVS_DETECTION_HIGH, &u8) == ESP_OK) {
            detection_high = u8;
        }

        gpio_config_t io_conf = { 0 };

        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT64(board_config.socket_lock_a_gpio) | BIT64(board_config.socket_lock_b_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = BIT64(board_config.socket_lock_detection_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        xTaskCreate(socket_lock_task_func, "socket_lock_task", 2 * 1024, NULL, 10, &socket_lock_task);
    }
}

bool socket_lock_is_detection_high(void)
{
    return detection_high;
}

void socket_lock_set_detection_high(bool _detection_high)
{
    detection_high = _detection_high;

    nvs_set_u8(nvs, NVS_DETECTION_HIGH, detection_high);
    nvs_commit(nvs);
}

uint16_t socket_lock_get_operating_time(void)
{
    return operating_time;
}

esp_err_t socket_lock_set_operating_time(uint16_t _operating_time)
{
    if (_operating_time < OPERATING_TIME_MIN || _operating_time > OPERATING_TIME_MAX) {
        ESP_LOGE(TAG, "Operating time out of range");
        return ESP_ERR_INVALID_ARG;
    }

    operating_time = _operating_time;
    nvs_set_u16(nvs, NVS_OPERATING_TIME, operating_time);
    nvs_commit(nvs);

    return ESP_OK;
}

uint8_t socket_lock_get_retry_count(void)
{
    return retry_count;
}

void socket_lock_set_retry_count(uint8_t _retry_count)
{
    retry_count = _retry_count;
    nvs_set_u8(nvs, NVS_RETRY_COUNT, retry_count);
    nvs_commit(nvs);
}

uint16_t socket_lock_get_break_time(void)
{
    return break_time;
}

esp_err_t socket_lock_set_break_time(uint16_t _break_time)
{
    if (_break_time < board_config.socket_lock_min_break_time) {
        ESP_LOGE(TAG, "Operating time out of range");
        return ESP_ERR_INVALID_ARG;
    }

    break_time = _break_time;
    nvs_set_u16(nvs, NVS_BREAK_TIME, break_time);
    nvs_commit(nvs);

    return ESP_OK;
}

void socket_lock_set_locked(bool locked)
{
    ESP_LOGI(TAG, "Set locked %d", locked);

    xTaskNotify(socket_lock_task, locked ? LOCK_BIT : UNLOCK_BIT, eSetBits);
    status = SOCKET_LOCK_STATUS_OPERATING;
}

socket_lock_status_t socket_lock_get_status(void)
{
    return status;
}