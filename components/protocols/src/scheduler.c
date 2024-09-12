
#include "scheduler.h"

#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <esp_sntp.h>
#include <nvs.h>
#include <time.h>

#include "evse.h"

#define NVS_NAMESPACE     "scheduler"
#define NVS_NTP_ENABLED   "ntp_en"
#define NVS_NTP_SERVER    "ntp_server"
#define NVS_NTP_FROM_DHCP "ntp_from_dhcp"
#define NVS_TIMEZONE      "timezone"
#define NVS_SCHEDULES     "schedules"

#define STATE_NONE 0
#define STATE_ON   1
#define STATE_OFF  2

static const char* TAG = "scheduler";

static nvs_handle nvs;

static SemaphoreHandle_t mutex;

static TaskHandle_t scheduler_task = NULL;

static char ntp_server[64];  // if renew_servers_after_new_IP = false, will be used static string reference

static uint8_t schedule_count = 0;

static scheduler_schedule_t* schedules = NULL;

static uint8_t* schedules_state = NULL;

static const char* tz_data[][2] = {
#include "tz_data.h"
    { NULL, NULL }
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

void ntp_sync_cb(struct timeval* tv)
{
    ESP_LOGD(TAG, "NTP sync");
    scheduler_execute_schedules();
}

static void on_action(scheduler_action_t action)
{
    switch (action) {
    case SCHEDULER_ACTION_ENABLE:
        evse_set_enabled(true);
        break;
    case SCHEDULER_ACTION_AVAILABLE:
        evse_set_available(true);
        break;
    case SCHEDULER_ACTION_CHARGING_CURRENT_6A:
        evse_set_charging_current(60);
        break;
    case SCHEDULER_ACTION_CHARGING_CURRENT_8A:
        evse_set_charging_current(80);
        break;
    case SCHEDULER_ACTION_CHARGING_CURRENT_10A:
        evse_set_charging_current(100);
        break;
    }
}

static void off_action(scheduler_action_t action)
{
    switch (action) {
    case SCHEDULER_ACTION_ENABLE:
        evse_set_enabled(false);
        break;
    case SCHEDULER_ACTION_AVAILABLE:
        evse_set_available(false);
        break;
    case SCHEDULER_ACTION_CHARGING_CURRENT_6A:
    case SCHEDULER_ACTION_CHARGING_CURRENT_8A:
    case SCHEDULER_ACTION_CHARGING_CURRENT_10A:
        evse_set_charging_current(evse_get_default_charging_current());
        break;
    }
}

static void scheduler_task_func(void* param)
{
    vTaskDelay(pdMS_TO_TICKS(1000));  // wait to init evse

    while (true) {
        xSemaphoreTake(mutex, portMAX_DELAY);

        time_t now = time(NULL);
        struct tm timeinfo = { 0 };
        localtime_r(&now, &timeinfo);

        for (uint8_t i = 0; i < schedule_count; i++) {
            uint32_t day = schedules[i].days.order[timeinfo.tm_wday];
            if (day & (1 << timeinfo.tm_hour)) {
                if (schedules_state[i] != STATE_ON) {
                    ESP_LOGI(TAG, "Trigger %s scheduler %d", "on", i);
                    on_action(schedules[i].action);
                    schedules_state[i] = STATE_ON;
                }
            } else {
                if (schedules_state[i] != STATE_OFF) {
                    ESP_LOGI(TAG, "Trigger %s scheduler %d", "off", i);
                    off_action(schedules[i].action);
                    schedules_state[i] = STATE_OFF;
                }
            }
        }

        timeinfo.tm_sec = 0;
        timeinfo.tm_min = 0;
        time_t next = mktime(&timeinfo);
        next += 3600;

        xSemaphoreGive(mutex);

        ulTaskNotifyTakeIndexed(0, pdTRUE, pdMS_TO_TICKS((next - now) * 1000));
    }
}

void scheduler_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    if (scheduler_is_ntp_enabled()) {
        scheduler_get_ntp_server(ntp_server);

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);
        config.sync_cb = ntp_sync_cb;

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

    size_t size = 0;
    nvs_get_blob(nvs, NVS_SCHEDULES, NULL, &size);
    if (size > 0) {
        if (size % sizeof(scheduler_schedule_t) > 0) {
            ESP_LOGW(TAG, "Schedules NVS incompatible size, schedules will be cleared");
        } else {
            schedule_count = size / sizeof(scheduler_schedule_t);
            schedules = (scheduler_schedule_t*)malloc(sizeof(scheduler_schedule_t) * schedule_count);
            nvs_get_blob(nvs, NVS_SCHEDULES, (void*)schedules, &size);

            schedules_state = (uint8_t*)realloc((void*)schedules_state, sizeof(uint8_t) * schedule_count);
            memset((void*)schedules_state, STATE_NONE, sizeof(uint8_t) * schedule_count);
        }
    }

    mutex = xSemaphoreCreateMutex();

    xTaskCreate(scheduler_task_func, "scheduler_task", 2 * 1024, NULL, 1, &scheduler_task);
}

void scheduler_execute_schedules(void)
{
    xTaskNotifyGive(scheduler_task);
}

uint8_t scheduler_get_schedule_count(void)
{
    return schedule_count;
}

scheduler_schedule_t* scheduler_get_schedules(void)
{
    return schedules;
}

void scheduler_set_schedule_config(const scheduler_schedule_t* _schedules, uint8_t _num_of_schedules)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    schedule_count = _num_of_schedules;
    if (schedule_count > 0) {
        schedules_state = (uint8_t*)realloc((void*)schedules_state, sizeof(uint8_t) * schedule_count);
        memset((void*)schedules_state, STATE_NONE, sizeof(uint8_t) * schedule_count);

        size_t size = sizeof(scheduler_schedule_t) * schedule_count;
        schedules = (scheduler_schedule_t*)realloc((void*)schedules, size);
        memcpy((void*)schedules, _schedules, size);
        nvs_set_blob(nvs, NVS_SCHEDULES, (void*)schedules, size);

        xTaskNotifyGive(scheduler_task);
    } else {
        free((void*)schedules_state);
        free((void*)schedules);
        nvs_erase_key(nvs, NVS_SCHEDULES);
    }

    nvs_commit(nvs);

    xSemaphoreGive(mutex);
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
        config.sync_cb = ntp_sync_cb;
        config.renew_servers_after_new_IP = from_dhcp;
        ret = esp_netif_sntp_init(&config);
    }

    if (ret == ESP_OK) {
        nvs_set_u8(nvs, NVS_NTP_ENABLED, enabled);
        nvs_set_str(nvs, NVS_NTP_SERVER, server);
        nvs_set_u8(nvs, NVS_NTP_FROM_DHCP, from_dhcp);
        nvs_commit(nvs);
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

const char* scheduler_action_to_str(scheduler_action_t action)
{
    switch (action) {
    case SCHEDULER_ACTION_AVAILABLE:
        return "available";
    case SCHEDULER_ACTION_CHARGING_CURRENT_6A:
        return "ch_cur_6a";
    case SCHEDULER_ACTION_CHARGING_CURRENT_8A:
        return "ch_cur_8a";
    case SCHEDULER_ACTION_CHARGING_CURRENT_10A:
        return "ch_cur_10a";
    default:
        return "enable";
    }
}

scheduler_action_t scheduler_str_to_action(const char* str)
{
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
    return SCHEDULER_ACTION_ENABLE;
}