#include <driver/uart.h>
#include <driver/uart_vfs.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_spiffs.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <unity.h>
#include <unity_fixture.h>

#include "evse.h"
#include "peripherals_mock.h"
#include "script_utils.h"

static void run_all_tests(void)
{
    RUN_TEST_GROUP(evse);
    RUN_TEST_GROUP(script);
}

// static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
//     }
// }

// void wifi_init(void)
// {
//     ESP_ERROR_CHECK(esp_netif_init());

//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = "ssid",
//             .password = "password",
//             .pmf_cfg = {
//                 .capable = true,
//                 .required = false,
//             },
//         },
//     };
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
// }

static void fs_info(esp_vfs_spiffs_conf_t* conf)
{
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(conf->partition_label, &total, &used);
    if (ret != ESP_OK) {
        printf("Failed to get partition %s information %s\n", conf->partition_label, esp_err_to_name(ret));
    } else {
        printf("Partition %s size: total: %d, used: %d\n", conf->partition_label, total, used);
    }
}

static void fs_init(void)
{
    esp_vfs_spiffs_conf_t cfg_conf = {
        .base_path = "/cfg",
        .partition_label = "cfg",
        .max_files = 1,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&cfg_conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("cfg Failed to mount or format filesystem\n");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            printf("cfg Failed to find SPIFFS partition\n");
        } else {
            printf("cfg Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret));
        }
        return;
    }

    //    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&cfg_conf));

    esp_vfs_spiffs_conf_t data_conf = {
        .base_path = "/data",
        .partition_label = "data",
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    ret = esp_vfs_spiffs_register(&data_conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("data Failed to mount or format filesystem\n");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            printf("data Failed to find SPIFFS partition\n");
        } else {
            printf("data Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret));
        }
        return;
    }
    //    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&data_conf));

    fs_info(&cfg_conf);
    fs_info(&data_conf);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        printf("Erasing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    fs_init();

    // wifi_init();

    script_mutex = xSemaphoreCreateMutex();

    evse_init();

    UNITY_MAIN_FUNC(run_all_tests);
}