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
#include <sys/param.h>

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

static void sta_try_connect(void)
{
    EventBits_t mode_bits = xEventGroupGetBits(wifi_event_group);
    if (!(mode_bits & WIFI_STA_SCAN_BIT) && mode_bits & WIFI_STA_MODE_BIT) {
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_connect returned 0x%x", err);
        }
    }
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            ESP_LOGI(TAG, "AP STA connected");
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
            sta_try_connect();
        }
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA start");
            sta_try_connect();
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

static esp_err_t wifi_restart(void)
{
    xEventGroupClearBits(wifi_event_group, WIFI_AP_CONNECTED_BIT | WIFI_STA_CONNECTED_BIT);

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_stop returned 0x%x", err);
        return err;
    }

    EventBits_t mode_bits = xEventGroupGetBits(wifi_event_group);

    wifi_mode_t mode;
    if (mode_bits & WIFI_AP_MODE_BIT) {
        mode = WIFI_MODE_APSTA;  // STA is need for scan
    } else if (mode_bits & WIFI_STA_MODE_BIT) {
        mode = WIFI_MODE_STA;
    } else {
        mode = WIFI_MODE_NULL;
    }

    err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode returned 0x%x", err);
        return err;
    }

    // STA config
    if (mode_bits & (WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT)) {
        // STA config is needed also in AP mode for scanning
        wifi_config_t sta_config = {
            .sta =
                {
                    .pmf_cfg =
                        {
                            .capable = true,
                            .required = false,
                        },
                },
        };
        if (mode_bits & WIFI_STA_MODE_BIT) {
            wifi_get_ssid((char*)sta_config.sta.ssid);
            wifi_get_password((char*)sta_config.sta.password);
        }

        err = esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_config returned 0x%x", err);
            return err;
        }
    }

    // AP config
    if (mode_bits & WIFI_AP_MODE_BIT) {
        wifi_config_t ap_config = {
            .ap =
                {
                    .max_connection = 1,
                    .authmode = WIFI_AUTH_OPEN,
                },
        };
        uint8_t mac[6];
        esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
        sprintf((char*)ap_config.ap.ssid, AP_SSID, mac[3], mac[4], mac[5]);

        err = esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_set_config returned 0x%x", err);
            return err;
        }
    }

    if (mode != WIFI_MODE_NULL) {
        err = esp_wifi_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_start returned 0x%x", err);
            return err;
        }
    }

    return ESP_OK;
}

void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ap_netif = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    // ESP_ERROR_CHECK(mdns_init());
    // ESP_ERROR_CHECK(mdns_hostname_set("evse"));
    // ESP_ERROR_CHECK(mdns_instance_name_set("EVSE controller"));

    if (wifi_is_enabled()) {
        xEventGroupSetBits(wifi_event_group, WIFI_STA_MODE_BIT);
    }
    wifi_restart();
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

    if (ssid != NULL && strlen(ssid) > WIFI_SSID_SIZE - 1) {
        ESP_LOGE(TAG, "SSID out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (password != NULL && strlen(password) > WIFI_PASSWORD_SIZE - 1) {
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

    if (enabled) {
        xEventGroupSetBits(wifi_event_group, WIFI_STA_MODE_BIT);
    } else {
        xEventGroupClearBits(wifi_event_group, WIFI_STA_MODE_BIT);
    }

    return wifi_restart();
}

wifi_scan_ap_list_t* wifi_scan_aps(void)
{
    EventBits_t mode_bits = xEventGroupGetBits(wifi_event_group);
    bool stopped = !(mode_bits & WIFI_STA_MODE_BIT || mode_bits & WIFI_STA_MODE_BIT);
    bool sta_connecting = mode_bits & WIFI_STA_MODE_BIT && mode_bits & WIFI_STA_DISCONNECTED_BIT;

    xEventGroupSetBits(wifi_event_group, WIFI_STA_SCAN_BIT);

    if (sta_connecting) {
        // during connecting scan is not able
        esp_wifi_disconnect();
    }

    if (stopped) {
        // need to start STA
        wifi_config_t sta_config = {
            .sta =
                {
                    .pmf_cfg =
                        {
                            .capable = true,
                            .required = false,
                        },
                },
        };
        esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
    }

    esp_wifi_scan_start(NULL, true);

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Found %d APs", ap_count);

    xEventGroupClearBits(wifi_event_group, WIFI_STA_SCAN_BIT);

    if (stopped) {
        esp_wifi_stop();
    }

    if (sta_connecting) {
        esp_wifi_connect();
    }

    wifi_scan_ap_list_t* list = (wifi_scan_ap_list_t*)malloc(sizeof(wifi_scan_ap_list_t));
    SLIST_INIT(list);

    wifi_ap_record_t ap;
    wifi_scan_ap_entry_t* last_entry = NULL;
    while (esp_wifi_scan_get_ap_record(&ap) == ESP_OK) {
        wifi_scan_ap_entry_t* entry = (wifi_scan_ap_entry_t*)malloc(sizeof(wifi_scan_ap_entry_t));
        entry->ssid = strdup((const char*)ap.ssid);
        entry->rssi = ap.rssi;
        entry->auth = ap.authmode != WIFI_AUTH_OPEN;

        if (last_entry) {
            SLIST_INSERT_AFTER(last_entry, entry, entries);
        } else {
            SLIST_INSERT_HEAD(list, entry, entries);
        }
        last_entry = entry;
    }

    return list;
}

void wifi_scan_aps_free(wifi_scan_ap_list_t* list)
{
    while (!SLIST_EMPTY(list)) {
        wifi_scan_ap_entry_t* item = SLIST_FIRST(list);

        SLIST_REMOVE_HEAD(list, entries);

        if (item->ssid) free((void*)item->ssid);
        free((void*)item);
    }

    free((void*)list);
}

bool wifi_is_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}

void wifi_get_ssid(char* value)
{
    size_t len = WIFI_SSID_SIZE - 1;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_SSID, value, &len);
}

bool wifi_has_ssid(void)
{
    size_t len = 0;
    nvs_get_str(nvs, NVS_SSID, NULL, &len);
    return len > 0;
}

void wifi_get_password(char* value)
{
    size_t len = WIFI_PASSWORD_SIZE - 1;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_PASSWORD, value, &len);
}

void wifi_ap_start(void)
{
    ESP_LOGI(TAG, "Starting AP");

    xEventGroupSetBits(wifi_event_group, WIFI_AP_MODE_BIT);
    wifi_restart();
}

void wifi_ap_stop(void)
{
    ESP_LOGI(TAG, "Stopping AP");

    xEventGroupClearBits(wifi_event_group, WIFI_AP_MODE_BIT);
    wifi_restart();
}

bool wifi_is_ap(void)
{
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    return mode == WIFI_MODE_APSTA;
}

void wifi_get_ip(bool ap, char* str)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap ? ap_netif : sta_netif, &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, str, 15);
}

void wifi_get_mac(bool ap, char* str)
{
    uint8_t mac[6];
    esp_wifi_get_mac(ap ? ESP_IF_WIFI_AP : ESP_IF_WIFI_STA, mac);
    sprintf(str, MACSTR, MAC2STR(mac));
}