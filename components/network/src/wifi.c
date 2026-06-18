#include "wifi.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <string.h>
#include <sys/param.h>

#define AP_SSID    "evse-%02x%02x%02x"
#define AP_CHANNEL 11

#define NVS_NAMESPACE      "wifi"
#define NVS_ENABLED        "enabled"
#define NVS_SSID           "ssid"
#define NVS_PASSWORD       "password"
#define NVS_STATIC_ENABLED "stat_enabled"
#define NVS_STATIC_IP      "stat_ip"
#define NVS_STATIC_GATEWAY "stat_gateway"
#define NVS_STATIC_NETMASK "stat_netmask"
#define NVS_STATIC_DNS     "stat_dns"

#define RETURN_ON_ERROR_LOG(x, msg)        \
    do {                                   \
        esp_err_t err_rc_ = (x);           \
        if (unlikely(err_rc_ != ESP_OK)) { \
            ESP_LOGE(TAG, msg);            \
            return err_rc_;                \
        }                                  \
    } while (0)

ESP_STATIC_ASSERT(sizeof(((wifi_config_t*)0)->sta.ssid) == WIFI_SSID_SIZE, "Wrong SSID size");
ESP_STATIC_ASSERT(sizeof(((wifi_config_t*)0)->sta.password) == WIFI_PASSWORD_SIZE, "Wrong password size");

static const char* TAG = "wifi";

static nvs_handle_t nvs;

static esp_netif_t* sta_netif;

static esp_netif_t* ap_netif;

static bool ap_enabled = false;

static bool sta_enabled = false;

static bool recconect = false;

EventGroupHandle_t wifi_event_group;

static void sta_try_connect(void)
{
    if (recconect) {
        ESP_LOGI(TAG, "STA reconnecting...");
        // during AP mode disable reconnecting, it disconect all clients from AP
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
            xEventGroupSetBits(wifi_event_group, WIFI_AP_CONNECTED_BIT);
        }
        if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            ESP_LOGI(TAG, "AP STA disconnected");
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
            ESP_LOGI(TAG, "WiFi AP " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
            xEventGroupClearBits(wifi_event_group, WIFI_AP_CONNECTED_BIT);
        }
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGI(TAG, "STA disconnected");
            xEventGroupClearBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
            sta_try_connect();
        }
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "STA start");
            // sta_try_connect();
        }
        if (event_id == WIFI_EVENT_STA_STOP) {
            ESP_LOGI(TAG, "STA stop");
        }
        if (event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "AP start");
        }
        if (event_id == WIFI_EVENT_AP_STOP) {
            ESP_LOGI(TAG, "AP stop");
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
            xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
        }
    }
}

static esp_err_t apply_mode_config()
{
    EventBits_t mode_bits = xEventGroupGetBits(wifi_event_group);

    recconect = false;

    wifi_mode_t mode;
    if (ap_enabled) {
        mode = WIFI_MODE_APSTA;  // STA is need for scan
    } else if (sta_enabled) {
        mode = WIFI_MODE_STA;
    } else {
        mode = WIFI_MODE_NULL;
    }

    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set mode failed (%s)", esp_err_to_name(err));
        return err;
    }

    // STA config
    if (sta_enabled || ap_enabled) {
        wifi_config_t prev_sta_config = { 0 };
        esp_wifi_get_config(WIFI_IF_STA, &prev_sta_config);

        // STA config is needed also in AP mode for scanning
        wifi_config_t sta_config = { 0 };

        if (sta_enabled /*&& sta_priority*/) {
            wifi_get_ssid((char*)sta_config.sta.ssid, sizeof(sta_config.sta.ssid));
            wifi_get_password((char*)sta_config.sta.password, sizeof(sta_config.sta.password));
        }

        if (!(mode_bits & WIFI_STA_CONNECTED_BIT) || (strcmp((char*)prev_sta_config.sta.ssid, (char*)sta_config.sta.ssid) != 0)) {
            // if connecting, disconnect (needs ESP32)
            // if changed SSID, disconnect
            esp_wifi_disconnect();
        }

        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Set config failed (%s)", esp_err_to_name(err));
            return err;
        }
    }

    // AP config
    if (ap_enabled) {
        wifi_config_t ap_config = {
            .ap =
                {
                    .max_connection = 1,
                    .authmode = WIFI_AUTH_OPEN,
                    .channel = AP_CHANNEL,
                    .pmf_cfg = {
                        .capable = true,
                        .required = false
                    },
                },
        };
        wifi_get_ap_ssid((char*)ap_config.ap.ssid, sizeof(ap_config.ap.ssid));

        err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Set config failed (%s)", esp_err_to_name(err));
            return err;
        }
    }

    recconect = !ap_enabled && sta_enabled;
    if (sta_enabled) {
        esp_wifi_connect();
    }

    return ESP_OK;
}

static esp_err_t apply_static_dns(esp_ip4_addr_t dns_addr)
{
    esp_netif_dns_info_t dns_info = { 0 };
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    dns_info.ip.u_addr.ip4 = dns_addr;

    if (dns_info.ip.u_addr.ip4.addr != 0) {
        esp_err_t err = esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Set DNS info failed (%s)", esp_err_to_name(err));
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

        esp_ip4_addr_t dns_addr = { 0 };
        size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, NVS_STATIC_DNS, &dns_addr, &size);

        ESP_ERROR_CHECK(apply_static_dns(dns_addr.addr ? dns_addr : ip_info.gw));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    sta_enabled = wifi_is_enabled();
    apply_mode_config();
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

    sta_enabled = enabled;
    return apply_mode_config();
}

wifi_scan_ap_list_t* wifi_scan_aps(void)
{
    if (!ap_enabled && !sta_enabled) {
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
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
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

void wifi_get_ssid(char* value, size_t value_size)
{
    size_t len = value_size;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_SSID, value, &len);
}

void wifi_get_ap_ssid(char* value, size_t value_size)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf(value, value_size, AP_SSID, mac[3], mac[4], mac[5]);
}

void wifi_get_password(char* value, size_t value_size)
{
    size_t len = value_size;
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

    ap_enabled = true;
    apply_mode_config();
}

void wifi_ap_stop(void)
{
    ESP_LOGI(TAG, "Stopping AP");

    ap_enabled = false;
    apply_mode_config();
}

void wifi_get_ip(bool ap, char* value, size_t value_size)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap ? ap_netif : sta_netif, &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, value, value_size);
}

void wifi_get_mac(bool ap, char* value, size_t value_size)
{
    uint8_t mac[6];
    esp_wifi_get_mac(ap ? WIFI_IF_AP : WIFI_IF_STA, mac);
    snprintf(value, value_size, MACSTR, MAC2STR(mac));
}

bool wifi_is_static_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_STATIC_ENABLED, &value);
    return value;
}

static void get_nvs_ip(const char* nvs_key, char* value, size_t value_size)
{
    esp_ip4_addr_t addr = { 0 };
    size_t size = sizeof(esp_ip4_addr_t);
    if (nvs_get_blob(nvs, nvs_key, &addr, &size) == ESP_OK && addr.addr != 0) {
        esp_ip4addr_ntoa(&addr, value, value_size);
    } else {
        value[0] = '\0';
    }
}

void wifi_get_static_ip(char* value, size_t value_size)
{
    get_nvs_ip(NVS_STATIC_IP, value, value_size);
}

void wifi_get_static_gateway(char* value, size_t value_size)
{
    get_nvs_ip(NVS_STATIC_GATEWAY, value, value_size);
}

void wifi_get_static_netmask(char* value, size_t value_size)
{
    get_nvs_ip(NVS_STATIC_NETMASK, value, value_size);
}

void wifi_get_static_dns(char* value, size_t value_size)
{
    get_nvs_ip(NVS_STATIC_DNS, value, value_size);
}

static esp_err_t apply_static_mode(bool enabled, const esp_netif_ip_info_t* ip_info)
{
    esp_netif_dhcp_status_t dhcp_status;
    esp_err_t err = esp_netif_dhcpc_get_status(sta_netif, &dhcp_status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DHCP client get status failed (%s)", esp_err_to_name(err));
    }

    bool current_enabled = dhcp_status != ESP_NETIF_DHCP_STARTED;
    if (enabled == current_enabled) {
        return ESP_OK;
    }

    if (!enabled) {
        err = esp_netif_dhcpc_start(sta_netif);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "DHCP client start failed (%s)", esp_err_to_name(err));
        }
        return err;
    }

    err = esp_netif_dhcpc_stop(sta_netif);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DHCP client stop failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = esp_netif_set_ip_info(sta_netif, ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set IP info failed (%s)", esp_err_to_name(err));
        esp_netif_dhcpc_start(sta_netif);  // best-effort revert to DHCP
    }
    return err;
}

static esp_err_t resolve_ipv4_addr(const char* str, const char* nvs_key, esp_ip4_addr_t* out)
{
    if (str == NULL) {
        size_t size = sizeof(esp_ip4_addr_t);
        nvs_get_blob(nvs, nvs_key, out, &size);
        return ESP_OK;
    }

    if (str[0] == '\0') {
        return ESP_OK;
    }

    return esp_netif_str_to_ip4(str, out);
}

esp_err_t wifi_set_static_config(bool enabled, const char* ip, const char* gateway, const char* netmask, const char* dns)
{
    esp_err_t err;
    esp_netif_ip_info_t ip_info = { 0 };
    esp_ip4_addr_t dns_addr = { 0 };

    RETURN_ON_ERROR_LOG(resolve_ipv4_addr(ip, NVS_STATIC_IP, &ip_info.ip), "Invalid IP");
    RETURN_ON_ERROR_LOG(resolve_ipv4_addr(gateway, NVS_STATIC_GATEWAY, &ip_info.gw), "Invalid gateway");
    RETURN_ON_ERROR_LOG(resolve_ipv4_addr(netmask, NVS_STATIC_NETMASK, &ip_info.netmask), "Invalid netmask");
    RETURN_ON_ERROR_LOG(resolve_ipv4_addr(dns, NVS_STATIC_DNS, &dns_addr), "Invalid DNS");

    if (enabled) {
        if (!ip_info.ip.addr) {
            ESP_LOGE(TAG, "Required IP");
            return ESP_ERR_INVALID_ARG;
        }
        if (!ip_info.gw.addr) {
            ESP_LOGE(TAG, "Required gateway");
            return ESP_ERR_INVALID_ARG;
        }
        if (!ip_info.netmask.addr) {
            ESP_LOGE(TAG, "Required netmask");
            return ESP_ERR_INVALID_ARG;
        }
    }

    err = apply_static_mode(enabled, &ip_info);
    if (err != ESP_OK) {
        return err;
    }

    if (enabled) {
        err = apply_static_dns(dns_addr.addr ? dns_addr : ip_info.gw);
        if (err != ESP_OK) {
            return err;
        }
    }

    nvs_set_u8(nvs, NVS_STATIC_ENABLED, enabled);
    nvs_set_blob(nvs, NVS_STATIC_IP, &ip_info.ip, sizeof(esp_ip4_addr_t));
    nvs_set_blob(nvs, NVS_STATIC_GATEWAY, &ip_info.gw, sizeof(esp_ip4_addr_t));
    nvs_set_blob(nvs, NVS_STATIC_NETMASK, &ip_info.netmask, sizeof(esp_ip4_addr_t));
    nvs_set_blob(nvs, NVS_STATIC_DNS, &dns_addr, sizeof(esp_ip4_addr_t));

    return ESP_OK;
}

bool wifi_is_ap_enabled(void)
{
    return ap_enabled;
}

bool wifi_is_sta_enabled(void)
{
    return sta_enabled;
}