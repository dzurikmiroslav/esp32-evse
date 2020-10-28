#include <string.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "mdns.h"
#include "driver/gpio.h"

#include "board_config.h"
#include "evse.h"
#include "peripherals.h"
#include "webserver.h"
#include "storage.h"

#define AP_CONNECTION_TIMEOUT       60000  // 60sec
#define RESET_HOLD_TIME             10000  // 10sec
#define NVS_NAME                    "evse"

#define WIFI_AP_SSID                "evse-%02x%02x%02x"
#define WIFI_AP_PASS                "12345678"
#define WIFI_STA_MAX_RETRY          3

// wifi task bits
#define CONNECT_BIT                 BIT0
#define CONNECTED_BIT               BIT1
#define DISCONNECTED_BIT            BIT2

// reset task bits
#define PRESS_BIT                   BIT0
#define RELEASED_BIT                BIT1

static const char *TAG = "main";

nvs_handle nvs;

static bool wifi_sta_enabled = false;

static TaskHandle_t wifi_task;

static TaskHandle_t reset_task;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "event_handler %s %d", event_base, event_id);
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "WiFi AP "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
            xTaskNotify(wifi_task, CONNECTED_BIT, eSetBits);
        }
        if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "WiFi AP "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
            xTaskNotify(wifi_task, DISCONNECTED_BIT, eSetBits);
        }
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            xTaskNotify(wifi_task, DISCONNECTED_BIT, eSetBits);
            esp_wifi_connect();
        }
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "WiFi STA got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            xTaskNotify(wifi_task, CONNECTED_BIT, eSetBits);
        }
    }
}

static void wifi_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
    ;

    assert(esp_netif_create_default_wifi_ap());
    assert(esp_netif_create_default_wifi_sta());

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // set AP config
    wifi_config_t wifi_config = { 0 };
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    sprintf((char*) wifi_config.ap.ssid, WIFI_AP_SSID, mac[3], mac[4], mac[5]);
    strcpy((char*) wifi_config.ap.password, WIFI_AP_PASS);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 1;

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    // set STA config
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    size_t ssid_len = sizeof(wifi_config.sta.ssid);
    nvs_get_str(nvs, NVS_WIFI_SSID, (char*) wifi_config.sta.ssid, &ssid_len);
    size_t password_len = sizeof(wifi_config.sta.password);
    nvs_get_str(nvs, NVS_WIFI_PASSWORD, (char*) wifi_config.sta.password, &password_len);

    uint8_t enabled = 1;
    nvs_get_u8(nvs, NVS_WIFI_ENABLED, &enabled);

    if (enabled) {
        if (strlen((char*) wifi_config.sta.ssid) > 0) {
            ESP_LOGI(TAG, "WiFi STA enabled");
            wifi_sta_enabled = true;
        } else {
            ESP_LOGW(TAG, "WiFi STA enabled, but no SSID");
        }
    } else {
        ESP_LOGI(TAG, "WiFi STA disabled");
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
}

static void init_mdns(void)
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(CONFIG_MDNS_HOST_NAME));
    ESP_ERROR_CHECK(mdns_instance_name_set("EVSE controller"));
}

static void wifi_task_func(void *param)
{
    uint32_t notification;
    wifi_mode_t mode = WIFI_MODE_NULL;
    bool connection = false;

    if (wifi_sta_enabled) {
        mode = WIFI_MODE_STA;
        ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
        ESP_ERROR_CHECK(esp_wifi_start());
        led_set_state(LED_ID_WIFI, LED_STATE_DUTY_50);
    }

    for (;;) {
        if (xTaskNotifyWait(0x00, 0xff, &notification, pdMS_TO_TICKS(AP_CONNECTION_TIMEOUT))) {
            if (notification & CONNECT_BIT) {
                if (mode != WIFI_MODE_AP) {
                    if (mode == WIFI_MODE_STA) {
                        ESP_ERROR_CHECK(esp_wifi_stop());
                    }
                    mode = WIFI_MODE_AP;
                    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
                    ESP_ERROR_CHECK(esp_wifi_start());
                    led_set_state(LED_ID_WIFI, LED_STATE_DUTY_5);
                }
            }
            if (notification & CONNECTED_BIT) {
                connection = true;
                led_set_state(LED_ID_WIFI, mode == WIFI_MODE_AP ? LED_STATE_DUTY_75 : LED_STATE_ON);
            }
            if (notification & DISCONNECTED_BIT) {
                connection = false;
                led_set_state(LED_ID_WIFI, mode == WIFI_MODE_AP ? LED_STATE_DUTY_5 : LED_STATE_DUTY_50);
            }
        } else {
            if (!connection && mode == WIFI_MODE_AP) {
                ESP_ERROR_CHECK(esp_wifi_stop());
                if (wifi_sta_enabled) {
                    mode = WIFI_MODE_STA;
                    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
                    ESP_ERROR_CHECK(esp_wifi_start());
                    led_set_state(LED_ID_WIFI, LED_STATE_DUTY_50);
                } else {
                    mode = WIFI_MODE_NULL;
                    led_set_state(LED_ID_WIFI, LED_STATE_OFF);
                }
            }
        }
    }
}

static void reset_task_func(void *param)
{
    uint32_t notification;
    bool pressed = false;

    for (;;) {
        if (xTaskNotifyWait(0x00, 0xff, &notification, pdMS_TO_TICKS(RESET_HOLD_TIME))) {
            if (notification & PRESS_BIT) {
                pressed = true;
            }
            if (notification & RELEASED_BIT) {
                pressed = false;
            }
        } else {
            if (pressed) {
                ESP_LOGW(TAG, "All settings will be erased...");

                ESP_ERROR_CHECK(nvs_erase_all(nvs));
                ESP_ERROR_CHECK(nvs_commit(nvs));

                ESP_LOGW(TAG, "Rebooting...");

                vTaskDelay(pdMS_TO_TICKS(500));

                esp_restart();
            }
        }
    }
}

static void IRAM_ATTR button_ap_isr_handler(void *arg)
{
    BaseType_t higher_task_woken = pdFALSE;

    if (gpio_get_level(board_config.button_wifi_gpio)) {
        xTaskNotifyFromISR(reset_task, RELEASED_BIT, eSetBits, &higher_task_woken);
    } else {
        xTaskNotifyFromISR(wifi_task, CONNECT_BIT, eSetBits, &higher_task_woken);
        xTaskNotifyFromISR(reset_task, PRESS_BIT, eSetBits, &higher_task_woken);
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
    gpio_isr_handler_add(board_config.button_wifi_gpio, button_ap_isr_handler, NULL);
}

esp_err_t init_spiffs(const char *partition)
{
    char base_path[64];
    sprintf(base_path, "/%s", partition);
// @formatter:off
    esp_vfs_spiffs_conf_t conf = {
            .base_path = base_path,
            .partition_label = partition,
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
    ret = esp_spiffs_info(partition, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(init_spiffs("cfg"));
    ESP_ERROR_CHECK(init_spiffs("www"));

    board_config_load();

    ESP_ERROR_CHECK(nvs_open(NVS_NAME, NVS_READWRITE, &nvs));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

    pilot_init();
    ac_relay_init();
    led_init();
    button_init();
    webserver_init();
    init_mdns();
    wifi_init();
    evse_init();

    xTaskCreate(wifi_task_func, "wifi_task", 4 * 1024, NULL, 10, &wifi_task);
    xTaskCreate(reset_task_func, "reset_task", 2 * 1024, NULL, 10, &reset_task);

//    ESP_ERROR_CHECK(dac_output_enable(DAC_CHANNEL_1));
//    ESP_ERROR_CHECK(dac_output_voltage(DAC_CHANNEL_1, 50));
//
//    esp_adc_cal_characteristics_t adc_char;
//    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
//    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_6));
//
//    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_6, ADC_WIDTH_BIT_12, 1100, &adc_char);
//
//    while (1) {
//        int adc_reading = adc1_get_raw(ADC1_CHANNEL_7);
//        uint32_t v = esp_adc_cal_raw_to_voltage(adc_reading, &adc_char);
//        ESP_LOGI(TAG, "U=%dmV raw=%d", v, adc_reading);
//
//        uint32_t vmult= 0;
//        int i;
//        for (i = 0; i < 20; i++) {
//            vmult += esp_adc_cal_raw_to_voltage(adc1_get_raw(ADC1_CHANNEL_7), &adc_char);
//        }
//        vmult /= i;
//        ESP_LOGI(TAG, "Umult=%dmV", vmult);
//
//        //TODO do stuff
//        vTaskDelay(pdMS_TO_TICKS(1000));
//    }

    evse_event_t evse_event;
    while (1) {
        if (xQueueReceive(evse_event_queue, &evse_event, portMAX_DELAY)) {
//            if (ble_notify_task) {
//                xTaskNotify(ble_notify_task, evse_event, eSetValueWithOverwrite);
//            }
        }
    }

}
