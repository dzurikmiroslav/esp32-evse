
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "nvs.h"

#include "scheduler.h"

#define NVS_NAMESPACE           "scheduler"
#define NVS_NTP_ENABLED         "ntp_en"
#define NVS_NTP_SERVER          "ntp_server"
#define NVS_NTP_FROM_DHCP       "ntp_from_dhcp"
#define NVS_TIMEZONE            "timezone"

static const char* TAG = "scheduler";

static nvs_handle nvs;

static char ntp_server[64]; // if renew_servers_after_new_IP = false, will be used static string reference

static scheduler_action_t actions[SCHEDULER_ID_MAX] = { 0 };

static uint32_t days[SCHEDULER_ID_MAX][7] = { 0 };

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

void scheduler_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    if (scheduler_is_ntp_enabled()) {
        scheduler_get_ntp_server(ntp_server);

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);

        esp_err_t ret = esp_netif_sntp_init(&config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SNTP init return %s", esp_err_to_name(ret));
        }
    }

    char str[64];
    scheduler_get_timezone(str);
    const char* tz = find_tz(str);
    if (tz) {
        setenv("TZ", tz, 1);
        tzset();
    } else {
        ESP_LOGW(TAG, "Unknown timezone %s", str);
    }
}

scheduler_action_t scheduler_get_action(uint8_t id)
{
    return actions[id];
}

uint32_t* scheduler_get_days(uint8_t id)
{
    return days[id];
}

esp_err_t scheduler_set_config(uint8_t id, scheduler_action_t action, uint32_t* days)
{
    if (id < 0 || id >= SCHEDULER_ID_MAX) {
        ESP_LOGE(TAG, "Scheduler id out of range");
        return ESP_ERR_INVALID_ARG;
    }

    actions[id] = action;
    memcpy(days[id], days, sizeof(uint32_t) * 7);

    return ESP_OK;
}

bool scheduler_is_ntp_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_NTP_ENABLED, &value);
    return value;
}

void scheduler_get_ntp_server(char* value)
{
    size_t len = 64;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_NTP_SERVER, value, &len);
}

bool scheduler_is_ntp_from_dhcp(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_NTP_FROM_DHCP, &value);
    return value;
}

esp_err_t scheduler_set_ntp_config(bool enabled, const char* server, bool from_dhcp)
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

void scheduler_get_timezone(char* value)
{
    size_t len = 64;
    value[0] = '\0';
    strcpy(value, "Etc/UTC");
    nvs_get_str(nvs, NVS_TIMEZONE, value, &len);
}

esp_err_t scheduler_set_timezone(const char* value)
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


/*
 SCHEDULER_ACTION_NONE,
    SCHEDULER_ACTION_ENABLE,
    SCHEDULER_ACTION_AVAILABLE,
    SCHEDULER_ACTION_CHARGING_CURRENT_6A,
    SCHEDULER_ACTION_CHARGING_CURRENT_8A,
    SCHEDULER_ACTION_CHARGING_CURRENT_10A
    */
const char* scheduler_action_to_str(scheduler_action_t action)
{
    switch (action)
    {
    case SCHEDULER_ACTION_ENABLE:
        return "enable";
    case SCHEDULER_ACTION_AVAILABLE:
        return "available";
    case SCHEDULER_ACTION_CHARGING_CURRENT_6A:
        return "ch_cur_6a";
    case SCHEDULER_ACTION_CHARGING_CURRENT_8A:
        return "ch_cur_8a";
    case SCHEDULER_ACTION_CHARGING_CURRENT_10A:
        return "ch_cur_10a";
    default:
        return "none";
    }
}

scheduler_action_t scheduler_str_to_action(const char* str)
{
    if (!strcmp(str, "enable")) {
        return SCHEDULER_ACTION_ENABLE;
    }
    if (!strcmp(str, "available")) {
        return SCHEDULER_ACTION_AVAILABLE;
    }
    if (!strcmp(str, "ch_cur_6a")) {
        return SCHEDULER_ACTION_CHARGING_CURRENT_6A;
    }
    if (!strcmp(str, "ch_cur_8a")) {
        return SCHEDULER_ACTION_CHARGING_CURRENT_8A;
    }
    if (!strcmp(str, "ch_cur_10a")) {
        return SCHEDULER_ACTION_CHARGING_CURRENT_10A;
    }
    return SCHEDULER_ACTION_NONE;
}