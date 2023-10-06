
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "nvs.h"

#include "date_time.h"

#define NVS_NAMESPACE           "date_time"
#define NVS_NTP_ENABLED         "ntp_en"
#define NVS_NTP_SERVER          "ntp_server"
#define NVS_NTP_FROM_DHCP       "ntp_from_dhcp"
#define NVS_TIMEZONE            "timezone"

static const char* TAG = "date_time";

static nvs_handle nvs;

static char ntp_server[64]; // if renew_servers_after_new_IP = false, will be used static string reference

static const char* tz_data[][2] = {
#include "tz_data.h"
    {NULL, NULL}
};

static const char* find_tz(const char* name)
{
    if (name != NULL) {
        int index = 0;
        while (true) {
            if (tz_data[index][0] == NULL) {
                return NULL;
            }
            if (strcmp(tz_data[index][0], name) == 0) {
                return tz_data[index][1];
            }
            index++;
        }
    }
    return NULL;
}

void date_time_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    if (date_time_is_ntp_enabled()) {
        date_time_get_ntp_server(ntp_server);

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);

        esp_err_t ret = esp_netif_sntp_init(&config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SNTP init return %s", esp_err_to_name(ret));
        }
    }

    char str[64];
    date_time_get_timezone(str);
    const char *tz = find_tz(str);
    if (tz) { 
        setenv("TZ", tz, 1);
        tzset();
    } else {
        ESP_LOGW(TAG, "Unknown timezone %s", str);
    }
}

bool date_time_is_ntp_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_NTP_ENABLED, &value);
    return value;
}

void date_time_get_ntp_server(char* value)
{
    size_t len = 64;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_NTP_SERVER, value, &len);
}

bool date_time_is_ntp_from_dhcp(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_NTP_FROM_DHCP, &value);
    return value;
}

esp_err_t date_time_set_ntp_config(bool enabled, const char* server, bool from_dhcp)
{
    esp_err_t ret = ESP_OK;

    esp_netif_sntp_deinit();
    if (enabled) {
        strcpy(ntp_server, server);
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);
        config.renew_servers_after_new_IP = from_dhcp;
        ret = esp_netif_sntp_init(&config);
    }

    if (ret == ESP_OK) {
        nvs_set_u8(nvs, NVS_NTP_ENABLED, enabled);
        nvs_set_str(nvs, NVS_NTP_SERVER, server);
        nvs_set_u8(nvs, NVS_NTP_FROM_DHCP, from_dhcp);
    }

    return ret;
}

void date_time_get_timezone(char* value)
{
    size_t len = 64;
    value[0] = '\0';
    strcpy(value, "Etc/UTC");
    nvs_get_str(nvs, NVS_TIMEZONE, value, &len);
}

esp_err_t date_time_set_timezone(const char* value)
{
    const char* tz = find_tz(value);

    if (tz) {
        setenv("TZ", tz, 1);
        tzset();

        nvs_set_str(nvs, NVS_TIMEZONE, value);

        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Unknown timezone %s", value);
        
        return ESP_ERR_INVALID_ARG;
    }
}