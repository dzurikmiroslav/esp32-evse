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

#define AP_SSID "evse-%02x%02x%02x"

#define NVS_NAMESPACE      "wifi"
#define NVS_ENABLED        "enabled"
#define NVS_SSID           "ssid"
#define NVS_PASSWORD       "password"
#define NVS_STATIC_ENABLED "stat_enabled"
#define NVS_STATIC_IP      "stat_ip"
#define NVS_STATIC_GATEWAY "stat_gateway"
#define NVS_STATIC_NETMASK "stat_netmask"

_Static_assert(sizeof(((wifi_config_t*)0)->sta.ssid) == WIFI_SSID_SIZE, "Wrong SSID size");
_Static_assert(sizeof(((wifi_config_t*)0)->sta.password) == WIFI_PASSWORD_SIZE, "Wrong password size");

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
            ESP_LOGE(TAG, "Connect failed (%s)", esp_err_to_name(err));
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
        ESP_LOGE(TAG, "Stop failed (%s)", esp_err_to_name(err));
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
        ESP_LOGE(TAG, "Set mode failed (%s)", esp_err_to_name(err));
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
            ESP_LOGE(TAG, "Set config failed (%s)", esp_err_to_name(err));
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
        wifi_get_ap_ssid((char*)ap_config.ap.ssid);

        err = esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Set config failed (%s)", esp_err_to_name(err));
            return err;
        }
    }

    if (mode != WIFI_MODE_NULL) {
        err = esp_wifi_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Start failed (%s)", esp_err_to_name(err));
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

    if (wifi_is_static_enabled()) {
        ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));

        esp_netif_ip_info_t ip_info = { 0 };

        size_t size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, NVS_STATIC_IP, &ip_info.ip, &size);

        size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, NVS_STATIC_GATEWAY, &ip_info.gw, &size);

        size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, NVS_STATIC_NETMASK, &ip_info.netmask, &size);

        ESP_ERROR_CHECK(esp_netif_set_ip_info(sta_netif, &ip_info));
    }

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
    bool stopped = !(mode_bits & WIFI_AP_MODE_BIT || mode_bits & WIFI_STA_MODE_BIT);
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

    xEventGroupClearBits(wifi_event_group, WIFI_STA_SCAN_BIT);

    if (stopped) {
        esp_wifi_stop();
    }

    if (sta_connecting) {
        esp_wifi_connect();
    }

    return list;
}

void wifi_scan_aps_free(wifi_scan_ap_list_t* list)
{
    while (!SLIST_EMPTY(list)) {
        wifi_scan_ap_entry_t* item = SLIST_FIRST(list);

        SLIST_REMOVE_HEAD(list, entries);

        wifi_scan_aps_entry_free(item);
    }

    free((void*)list);
}

void wifi_scan_aps_entry_free(wifi_scan_ap_entry_t* entry)
{
    if (entry->ssid) free((void*)entry->ssid);
    free((void*)entry);
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

void wifi_get_ap_ssid(char* value)
{
    uint8_t mac[6];
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    sprintf(value, AP_SSID, mac[3], mac[4], mac[5]);
}

void wifi_get_password(char* value)
{
    size_t len = WIFI_PASSWORD_SIZE - 1;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_PASSWORD, value, &len);
}

int8_t wifi_get_rssi(void)
{
    wifi_ap_record_t ap_record = { 0 };
    esp_wifi_sta_get_ap_info(&ap_record);

    return ap_record.rssi;
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
    esp_ip4addr_ntoa(&ip_info.ip, str, 16);
}

void wifi_get_mac(bool ap, char* str)
{
    uint8_t mac[6];
    esp_wifi_get_mac(ap ? ESP_IF_WIFI_AP : ESP_IF_WIFI_STA, mac);
    sprintf(str, MACSTR, MAC2STR(mac));
}

bool wifi_is_static_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_STATIC_ENABLED, &value);
    return value;
}

static void get_nvs_ip(const char* nvs_key, char* str)
{
    esp_ip4_addr_t value = { 0 };
    size_t size = sizeof(esp_ip4_addr_t);
    if (nvs_get_blob(nvs, nvs_key, &value, &size) == ESP_OK) {
        esp_ip4addr_ntoa(&value, str, 16);
    } else {
        str[0] = '\0';
    }
}

void wifi_get_static_ip(char* str)
{
    get_nvs_ip(NVS_STATIC_IP, str);
}

void wifi_get_static_gateway(char* str)
{
    get_nvs_ip(NVS_STATIC_GATEWAY, str);
}

void wifi_get_static_netmask(char* str)
{
    get_nvs_ip(NVS_STATIC_NETMASK, str);
}

esp_err_t wifi_set_static_config(bool enabled, const char* ip, const char* gateway, const char* netmask)
{
    esp_err_t err;
    size_t size;
    esp_netif_ip_info_t ip_info = { 0 };

    if (ip != NULL) {
        err = esp_netif_str_to_ip4(ip, &ip_info.ip);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Invalid IP");
            return err;
        }
    } else {
        size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, NVS_STATIC_IP, &ip_info.ip, &size);
    }

    if (gateway != NULL) {
        err = esp_netif_str_to_ip4(gateway, &ip_info.gw);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Invalid gateway");
            return err;
        }
    } else {
        size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, NVS_STATIC_GATEWAY, &ip_info.gw, &size);
    }

    if (netmask != NULL) {
        err = esp_netif_str_to_ip4(netmask, &ip_info.netmask);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Invalid netmask");
            return err;
        }
    } else {
        size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, NVS_STATIC_NETMASK, &ip_info.netmask, &size);
    }

    esp_netif_dhcp_status_t dhcp_status;
    err = esp_netif_dhcpc_get_status(sta_netif, &dhcp_status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DHCP client get status failed (%s)", esp_err_to_name(err));
    }

    bool current_enabled = dhcp_status != ESP_NETIF_DHCP_STARTED;

    if (enabled != current_enabled) {
        if (enabled) {
            err = esp_netif_dhcpc_stop(sta_netif);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "DHCP client stop failed (%s)", esp_err_to_name(err));
                return err;
            }

            err = esp_netif_set_ip_info(sta_netif, &ip_info);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Set IP info failed (%s)", esp_err_to_name(err));

                err = esp_netif_dhcpc_start(sta_netif);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "DHCP client restart failed (%s)", esp_err_to_name(err));
                    return err;
                }

                return err;
            }
        } else {
            err = esp_netif_dhcpc_start(sta_netif);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "DHCP client start failed (%s)", esp_err_to_name(err));
                return err;
            }
        }
    }

    if (enabled) {
        err = esp_netif_set_ip_info(sta_netif, &ip_info);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Set IP info failed (%s)", esp_err_to_name(err));
            return err;
        }
    }

    nvs_set_u8(nvs, NVS_STATIC_ENABLED, enabled);
    nvs_set_blob(nvs, NVS_STATIC_IP, &ip_info.ip, sizeof(esp_ip4_addr_t));
    nvs_set_blob(nvs, NVS_STATIC_GATEWAY, &ip_info.gw, sizeof(esp_ip4_addr_t));
    nvs_set_blob(nvs, NVS_STATIC_NETMASK, &ip_info.netmask, sizeof(esp_ip4_addr_t));

    return ESP_OK;
}