#include "wifi.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>
// #include <mdns.h>

#define AP_SSID "evse-%02x%02x%02x"

#define NVS_NAMESPACE "wifi"
#define NVS_ENABLED   "enabled"
#define NVS_SSID      "ssid"
#define NVS_PASSWORD  "password"

static const char* TAG = "wifi";

static nvs_handle_t nvs;

static esp_netif_t* sta_netif;

static esp_netif_t* ap_netif;

EventGroupHandle_t wifi_event_group;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            ESP_LOGI(TAG, "STA connected");
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
            ESP_LOGI(TAG, "WiFi AP " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
            xEventGroupClearBits(wifi_event_group, WIFI_AP_DISCONNECTED_BIT);
            xEventGroupSetBits(wifi_event_group, WIFI_AP_CONNECTED_BIT);
        }
        if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            ESP_LOGI(TAG, "AP STA disconnected");
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
            ESP_LOGI(TAG, "WiFi AP " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
            xEventGroupClearBits(wifi_event_group, WIFI_AP_CONNECTED_BIT);
            xEventGroupSetBits(wifi_event_group, WIFI_AP_DISCONNECTED_BIT);
        }
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG, "STA disconnected");
            xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
            xEventGroupSetBits(wifi_event_group, WIFI_STA_DISCONNECTED_BIT);
            esp_wifi_connect();
        }
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA start");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP || event_id == IP_EVENT_GOT_IP6) {
            if (event_id == IP_EVENT_STA_GOT_IP) {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
                ESP_LOGI(TAG, "WiFi STA got ip: " IPSTR, IP2STR(&event->ip_info.ip));
            } else {
                ip_event_got_ip6_t* event = (ip_event_got_ip6_t*)event_data;
                ESP_LOGI(TAG, "WiFi STA got ip6: " IPV6STR, IPV62STR(event->ip6_info.ip));
            }
            xEventGroupClearBits(wifi_event_group, WIFI_STA_DISCONNECTED_BIT);
            xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
        }
    }
}

static void sta_set_config(void)
{
    if (wifi_get_enabled()) {
        wifi_config_t wifi_config = {
            .sta =
                {
                    .pmf_cfg =
                        {
                            .capable = true,
                            .required = false,
                        },
                },
        };
        wifi_get_ssid((char*)wifi_config.sta.ssid);
        wifi_get_password((char*)wifi_config.sta.password);

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    }
}

static void ap_set_config(void)
{
    wifi_config_t wifi_ap_config = {
        .ap =
            {
                .max_connection = 1,
                .authmode = WIFI_AUTH_OPEN,
            },
    };
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    sprintf((char*)wifi_ap_config.ap.ssid, AP_SSID, mac[3], mac[4], mac[5]);

    wifi_config_t wifi_sta_config = { 0 };

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_ap_config);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config);
}

static void sta_try_start(void)
{
    sta_set_config();
    if (wifi_get_enabled()) {
        ESP_LOGI(TAG, "Starting STA");
        esp_wifi_start();
        xEventGroupSetBits(wifi_event_group, WIFI_STA_MODE_BIT);
    }
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ap_netif = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    // ESP_ERROR_CHECK(mdns_init());
    // ESP_ERROR_CHECK(mdns_hostname_set("evse"));
    // ESP_ERROR_CHECK(mdns_instance_name_set("EVSE controller"));

    sta_try_start();
}

esp_netif_t* wifi_get_sta_netif(void)
{
    return sta_netif;
}

esp_netif_t* wifi_get_ap_netif(void)
{
    return ap_netif;
}

esp_err_t wifi_set_config(bool enabled, const char* ssid, const char* password)
{
    if (enabled) {
        if (ssid == NULL || strlen(ssid) == 0) {
            size_t len = 0;
            nvs_get_str(nvs, NVS_SSID, NULL, &len);
            if (len <= 1) {
                ESP_LOGE(TAG, "Required SSID");
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    if (ssid != NULL && strlen(ssid) > 32) {
        ESP_LOGE(TAG, "SSID out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (password != NULL && strlen(password) > 32) {
        ESP_LOGE(TAG, "Password out of range");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_set_u8(nvs, NVS_ENABLED, enabled);
    if (ssid != NULL) {
        nvs_set_str(nvs, NVS_SSID, ssid);
    }
    if (password != NULL) {
        nvs_set_str(nvs, NVS_PASSWORD, password);
    }
    nvs_commit(nvs);

    ESP_LOGI(TAG, "Stopping AP/STA");
    xEventGroupClearBits(wifi_event_group, WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT);
    esp_wifi_stop();

    sta_try_start();

    return ESP_OK;
}

uint16_t wifi_scan(wifi_scan_ap_t* scan_aps)
{
    uint16_t number = WIFI_SCAN_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[WIFI_SCAN_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&ap_count);
    esp_wifi_scan_get_ap_records(&number, ap_info);

    for (int i = 0; (i < WIFI_SCAN_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        strcpy(scan_aps[i].ssid, (const char*)ap_info[i].ssid);
        scan_aps[i].rssi = ap_info[i].rssi;
        scan_aps[i].auth = ap_info[i].authmode != WIFI_AUTH_OPEN;
    }

    return ap_count;
}

bool wifi_get_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}

void wifi_get_ssid(char* value)
{
    size_t len = 32;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_SSID, value, &len);
}

void wifi_get_password(char* value)
{
    size_t len = 64;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_PASSWORD, value, &len);
}

void wifi_ap_start(void)
{
    ESP_LOGI(TAG, "Starting AP");

    xEventGroupClearBits(wifi_event_group, WIFI_STA_MODE_BIT);
    esp_wifi_stop();

    ap_set_config();
    esp_wifi_start();

    xEventGroupSetBits(wifi_event_group, WIFI_AP_MODE_BIT);
}

void wifi_ap_stop(void)
{
    ESP_LOGI(TAG, "Stopping AP");
    xEventGroupClearBits(wifi_event_group, WIFI_AP_MODE_BIT);
    esp_wifi_stop();

    sta_try_start();
}

bool wifi_is_ap(void)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    return mode == WIFI_MODE_APSTA;
}
