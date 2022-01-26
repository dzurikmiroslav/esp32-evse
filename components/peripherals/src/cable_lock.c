#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs.h"

#include "cable_lock.h"
#include "board_config.h"

#define NVS_NAMESPACE           "cable_lock"
#define NVS_TYPE                "type"

static const char* TAG = "cable_lock";

static nvs_handle_t nvs;

static cable_lock_type_t type = CABLE_LOCK_TYPE_NONE;

static bool lock = false;

void cable_lock_init(void)
{
    if (board_config.cable_lock) {
        ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

        uint8_t u8;
        if (nvs_get_u8(nvs, NVS_TYPE, &u8) == ESP_OK) {
            type = u8;
        }

        gpio_config_t io_conf;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << board_config.cable_lock_a_gpio) | (1ULL << board_config.cable_lock_b_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = 1ULL << board_config.cable_lock_test_gpio;
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }
}

cable_lock_type_t cable_lock_get_type(void)
{
    return type;
}

void cable_lock_set_type(cable_lock_type_t value)
{
    if (board_config.cable_lock) {
        type = value;
        nvs_set_u8(nvs, NVS_TYPE, type);

        nvs_commit(nvs);
    }
}

static void lock_task_func(void* param)
{
    while ((type == CABLE_LOCK_TYPE_MOTOR || type == CABLE_LOCK_TYPE_SOLENOID) && lock) {
        gpio_set_level(board_config.cable_lock_a_gpio, 1);
        gpio_set_level(board_config.cable_lock_a_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(type == CABLE_LOCK_TYPE_MOTOR ? 300 : 600));
        gpio_set_level(board_config.cable_lock_a_gpio, 0);
        gpio_set_level(board_config.cable_lock_a_gpio, 0);

        if (!gpio_get_level(board_config.cable_lock_test_gpio)) {
            ESP_LOGI(TAG, "Cable locked");
            break;
        }
        ESP_LOGW(TAG, "Cable not locked try again after 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    vTaskDelete(NULL);
}

static void unlock_task_func(void* param)
{
    while ((type == CABLE_LOCK_TYPE_MOTOR || type == CABLE_LOCK_TYPE_SOLENOID) && !lock) {
        gpio_set_level(board_config.cable_lock_a_gpio, 0);
        gpio_set_level(board_config.cable_lock_a_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(type == CABLE_LOCK_TYPE_MOTOR ? 300 : 600));
        gpio_set_level(board_config.cable_lock_a_gpio, 0);
        gpio_set_level(board_config.cable_lock_a_gpio, 0);

        if (gpio_get_level(board_config.cable_lock_test_gpio)) {
            ESP_LOGI(TAG, "Cable unlocked");
            break;
        }
        ESP_LOGW(TAG, "Cable not unlocked try again after 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    vTaskDelete(NULL);
}

void cable_lock_lock(void)
{
    if (!lock) {
        lock = true;
        xTaskCreate(lock_task_func, "lock_task", 2 * 1024, NULL, 10, NULL);
    }
}

void cable_lock_unlock(void)
{
    if (lock) {
        lock = false;
        xTaskCreate(unlock_task_func, "unlock_task", 2 * 1024, NULL, 10, NULL);
    }
}
