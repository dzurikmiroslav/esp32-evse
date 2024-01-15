#include <string.h>
#include <stdbool.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"

#include "evse.h"
#include "peripherals.h"
#include "led.h"
#include "modbus.h"
#include "protocols.h"
#include "serial.h"
#include "board_config.h"
#include "wifi.h"
#include "script.h"
#include "logger.h"


#define AP_CONNECTION_TIMEOUT   60000 // 60sec
#define RESET_HOLD_TIME         10000 // 10sec

#define PRESS_BIT               BIT0
#define RELEASED_BIT            BIT1

static const char* TAG = "app_main";

static TaskHandle_t user_input_task;

static evse_state_t led_state = -1;

static void reset_and_reboot(void)
{
    ESP_LOGW(TAG, "All settings will be erased...");
    ESP_ERROR_CHECK(nvs_flash_erase());

    ESP_LOGW(TAG, "Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_restart();
}

static void wifi_event_task_func(void* param)
{
    EventBits_t mode_bits;

    while (true) {
        led_set_off(LED_ID_WIFI);
        mode_bits = xEventGroupWaitBits(wifi_event_group, WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if (mode_bits & WIFI_AP_MODE_BIT) {
            led_set_state(LED_ID_WIFI, 100, 900);

            if (xEventGroupWaitBits(wifi_event_group, WIFI_AP_CONNECTED_BIT | WIFI_STA_MODE_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(AP_CONNECTION_TIMEOUT)) & WIFI_AP_CONNECTED_BIT) {
                led_set_state(LED_ID_WIFI, 1900, 100);
                do {
                } while (!(xEventGroupWaitBits(wifi_event_group, WIFI_AP_DISCONNECTED_BIT | WIFI_STA_MODE_BIT, pdFALSE, pdFALSE, portMAX_DELAY) & WIFI_AP_DISCONNECTED_BIT));
            } else {
                if (xEventGroupGetBits(wifi_event_group) & WIFI_AP_MODE_BIT) {
                    wifi_ap_stop();
                }
            }
        } else if (mode_bits & WIFI_STA_MODE_BIT) {
            led_set_state(LED_ID_WIFI, 500, 500);

            if (xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT | WIFI_AP_MODE_BIT, pdFALSE, pdFALSE, portMAX_DELAY) & WIFI_STA_CONNECTED_BIT) {
                led_set_on(LED_ID_WIFI);
                do {
                } while (!(xEventGroupWaitBits(wifi_event_group, WIFI_STA_DISCONNECTED_BIT | WIFI_AP_MODE_BIT, pdFALSE, pdFALSE, portMAX_DELAY) & WIFI_STA_DISCONNECTED_BIT));
            }
        }
    }
}

static void user_input_task_func(void* param)
{
    uint32_t notification;

    bool pressed = false;
    TickType_t press_tick = 0;

    while (true) {
        if (xTaskNotifyWait(0x00, 0xff, &notification, portMAX_DELAY)) {
            if (notification & PRESS_BIT) {
                press_tick = xTaskGetTickCount();
                pressed = true;
            }
            if (notification & RELEASED_BIT) {
                if (pressed) { // sometimes after connect debug UART emit RELEASED_BIT without preceding PRESS_BIT
                    if (xTaskGetTickCount() - press_tick >= pdMS_TO_TICKS(RESET_HOLD_TIME)) {
                        evse_set_available(false);
                        reset_and_reboot();
                    } else {
                        if (!(xEventGroupGetBits(wifi_event_group) & WIFI_AP_MODE_BIT)) {
                            wifi_ap_start();
                        }
                    }
                }
                pressed = false;
            }
        }
    }
}

static void IRAM_ATTR button_isr_handler(void* arg)
{
    BaseType_t higher_task_woken = pdFALSE;

    if (gpio_get_level(board_config.button_wifi_gpio)) {
        xTaskNotifyFromISR(user_input_task, RELEASED_BIT, eSetBits, &higher_task_woken);
    } else {
        xTaskNotifyFromISR(user_input_task, PRESS_BIT, eSetBits, &higher_task_woken);
    }

    if (higher_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void button_init(void)
{
    gpio_config_t conf = {
        .pin_bit_mask = BIT64(board_config.button_wifi_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&conf));
    ESP_ERROR_CHECK(gpio_isr_handler_add(board_config.button_wifi_gpio, button_isr_handler, NULL));
}

static void fs_info(esp_vfs_spiffs_conf_t* conf)
{
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(conf->partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition %s information %s", conf->partition_label, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition %s size: total: %d, used: %d", conf->partition_label, total, used);
    }
}

static void fs_init(void)
{
    esp_vfs_spiffs_conf_t cfg_conf = {
        .base_path = "/cfg",
        .partition_label = "cfg",
        .max_files = 1,
        .format_if_mount_failed = false
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&cfg_conf));

    esp_vfs_spiffs_conf_t data_conf = {
        .base_path = "/data",
        .partition_label = "data",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&data_conf));

    fs_info(&cfg_conf);
    fs_info(&data_conf);
}

static bool ota_diagnostic(void)
{
    //TODO diagnostic after ota
    return true;
}

static void update_leds(void)
{
    if (led_state != evse_get_state()) {
        led_state = evse_get_state();

        switch (led_state) {
        case EVSE_STATE_A:
            led_set_off(LED_ID_CHARGING);
            led_set_off(LED_ID_ERROR);
            break;
        case EVSE_STATE_B1:
        case EVSE_STATE_B2:
            led_set_state(LED_ID_CHARGING, 500, 500);
            led_set_off(LED_ID_ERROR);
            break;
        case EVSE_STATE_C1:
        case EVSE_STATE_D1:
            led_set_state(LED_ID_CHARGING, 1900, 100);
            led_set_off(LED_ID_ERROR);
            break;
        case EVSE_STATE_C2:
        case EVSE_STATE_D2:
            led_set_on(LED_ID_CHARGING);
            led_set_off(LED_ID_ERROR);
            break;
        case EVSE_STATE_E:
            led_set_off(LED_ID_CHARGING);
            led_set_on(LED_ID_ERROR);
            break;
        case EVSE_STATE_F:
            led_set_off(LED_ID_CHARGING);
            led_set_state(LED_ID_ERROR, 500, 500);
            break;
        }
    }
}

void app_main(void)
{
    logger_init();
    esp_log_set_vprintf(logger_vprintf);

    const esp_partition_t* running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Running partition: %s", running->label);

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "OTA pending verify");
            if (ota_diagnostic()) {
                ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    fs_init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    board_config_load();

    wifi_init();
    peripherals_init();
    modbus_init();
    serial_init();
    protocols_init();
    evse_init();
    button_init();
    script_init();

    xTaskCreate(wifi_event_task_func, "wifi_event_task", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(user_input_task_func, "user_input_task", 2 * 1024, NULL, 5, &user_input_task);

    while (true) {
        evse_process();
        update_leds();

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}