#include <string.h>
#include <stdbool.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "evse.h"
#include "led.h"
#include "pilot.h"
#include "relay.h"
#include "cable_lock.h"
#include "energy_meter.h"
#include "board_config.h"
#include "mqtt.h"
#include "webserver.h"
#include "wifi.h"

#define AP_CONNECTION_TIMEOUT       60000  // 60sec
#define RESET_HOLD_TIME             10000  // 10sec

#define PRESS_BIT                   BIT0
#define RELEASED_BIT                BIT1

static const char *TAG = "app_main";

static TaskHandle_t user_input_task;

static evse_state_t state;

static bool enabled;

static void reset_and_reboot(void)
{
    ESP_LOGW(TAG, "All settings will be erased...");
    ESP_ERROR_CHECK(nvs_flash_erase());

    ESP_LOGW(TAG, "Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(500));

    esp_restart();
}

static void wifi_event_task_func(void *param)
{
    EventBits_t mode_bits;
    EventBits_t connect_bits;

    for (;;) {
        led_set_off(LED_ID_WIFI);
        mode_bits = xEventGroupWaitBits(wifi_mode_event_group, WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        if (mode_bits & WIFI_AP_MODE_BIT) {
            led_set_state(LED_ID_WIFI, 150, 150);

            connect_bits = xEventGroupWaitBits(wifi_ap_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(AP_CONNECTION_TIMEOUT));
            if (connect_bits & WIFI_CONNECTED_BIT) {
                led_set_state(LED_ID_WIFI, 1900, 100);
                do {
                } while ( WIFI_DISCONNECTED_BIT != xEventGroupWaitBits(wifi_ap_event_group, WIFI_DISCONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY));
            } else {
                if (xEventGroupGetBits(wifi_mode_event_group) & WIFI_AP_MODE_BIT) {
                    wifi_ap_stop();
                }
            }
        } else if (mode_bits & WIFI_STA_MODE_BIT) {
            led_set_state(LED_ID_WIFI, 500, 500);

            connect_bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
            if (connect_bits & WIFI_CONNECTED_BIT) {
                led_set_on(LED_ID_WIFI);
                do {
                } while (WIFI_DISCONNECTED_BIT != xEventGroupWaitBits(wifi_event_group, WIFI_DISCONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY));
            }
        }
    }
}

static void user_input_task_func(void *param)
{
    uint32_t notification;

    bool pressed = false;
    TickType_t press_tick = 0;

    for (;;) {
        if (xTaskNotifyWait(0x00, 0xff, &notification, portMAX_DELAY)) {
            if (notification & PRESS_BIT) {
                press_tick = xTaskGetTickCount();
                pressed = true;
            }
            if (notification & RELEASED_BIT) {
                if (pressed) { // sometimes after connect debug UART emit RELEASED_BIT without preceding PRESS_BIT
                    if (xTaskGetTickCount() - press_tick >= pdMS_TO_TICKS(RESET_HOLD_TIME)) {
                        evse_disable(EVSE_DISABLE_BIT_SYSTEM);
                        reset_and_reboot();
                    } else {
                        if (!(xEventGroupGetBits(wifi_mode_event_group) & WIFI_AP_MODE_BIT)) {
                            wifi_ap_start();
                        }
                    }
                }
                pressed = false;
            }
        }
    }
}

static void IRAM_ATTR button_isr_handler(void *arg)
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
    gpio_pad_select_gpio(board_config.button_wifi_gpio);
    gpio_set_direction(board_config.button_wifi_gpio, GPIO_MODE_INPUT);
    gpio_set_pull_mode(board_config.button_wifi_gpio, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(board_config.button_wifi_gpio, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(board_config.button_wifi_gpio, button_isr_handler, NULL);
}

static esp_err_t init_spiffs()
{
// @formatter:off
    esp_vfs_spiffs_conf_t conf = {
            .base_path = "/cfg",
            .partition_label = "cfg",
            .max_files = 5,
            .format_if_mount_failed = false
    };
// @formatter:on
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("cfg", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

static bool ota_diagnostic(void)
{
    //TODO diagnostic after ota
    return true;
}

static void set_led_state()
{
    if (enabled != evse_is_enabled() || state != evse_get_state()) {
        enabled = evse_is_enabled();
        state = evse_get_state();

        if (enabled) {
            switch (state) {
                case EVSE_STATE_A:
                    led_set_state(LED_ID_CHARGING, 100, 1900);
                    led_set_off(LED_ID_ERROR);
                    break;
                case EVSE_STATE_B:
                    led_set_state(LED_ID_CHARGING, 250, 250);
                    led_set_off(LED_ID_ERROR);
                    break;
                case EVSE_STATE_C:
                case EVSE_STATE_D:
                    led_set_on(LED_ID_CHARGING);
                    led_set_off(LED_ID_ERROR);
                    break;
                case EVSE_STATE_E:
                case EVSE_STATE_F:
                    led_set_off(LED_ID_CHARGING);
                    led_set_on(LED_ID_ERROR);
                    break;
            }
        } else {
            led_set_off(LED_ID_CHARGING);
            led_set_off(LED_ID_ERROR);
        }
    }
}

void app_main()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
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

    ESP_ERROR_CHECK(init_spiffs());

    board_config_load();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));

    pilot_init();
    ac_relay_init();
    cable_lock_init();
    energy_meter_init();
    led_init();
    button_init();
    webserver_init();
    wifi_init();
    mqtt_init();
    evse_init();

    xTaskCreate(wifi_event_task_func, "wifi_event_task", 4 * 1024, NULL, 10, NULL);
    xTaskCreate(user_input_task_func, "user_input_task", 2 * 1024, NULL, 10, &user_input_task);

    TickType_t tick;

    for (;;) {
        tick = xTaskGetTickCount();

        evse_process();
        energy_meter_process();
        mqtt_process();
        set_led_state();

        vTaskDelay(MAX(0, pdMS_TO_TICKS(1000) - (xTaskGetTickCount() - tick)));
    }
}
