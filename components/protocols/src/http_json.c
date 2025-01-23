#include "http_json.h"

#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <string.h>
#include <sys/time.h>

#include "sdkconfig.h"

#include "board_config.h"
#include "energy_meter.h"
#include "evse.h"
#include "modbus.h"
#include "modbus_tcp.h"
#include "proximity.h"
#include "scheduler.h"
#include "script.h"
#include "serial.h"
#include "socket_lock.h"
#include "temp_sensor.h"
#include "wifi.h"

#define RETURN_ON_ERROR(x)                 \
    do {                                   \
        esp_err_t err_rc_ = (x);           \
        if (unlikely(err_rc_ != ESP_OK)) { \
            return err_rc_;                \
        }                                  \
    } while (0)

typedef struct {
    bool enabled;
    bool ssid_blank;
    char ssid[32];
    bool password_blank;
    char password[64];
} wifi_set_config_arg_t;

cJSON* http_json_get_evse_config(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "maxChargingCurrent", evse_get_max_charging_current());
    cJSON_AddNumberToObject(json, "chargingCurrent", evse_get_charging_current() / 10.0);
    cJSON_AddNumberToObject(json, "defaultChargingCurrent", evse_get_default_charging_current() / 10.0);
    cJSON_AddBoolToObject(json, "requireAuth", evse_is_require_auth());
    cJSON_AddBoolToObject(json, "socketOutlet", evse_get_socket_outlet());
    cJSON_AddBoolToObject(json, "rcm", evse_is_rcm());
    cJSON_AddNumberToObject(json, "temperatureThreshold", evse_get_temp_threshold());
    cJSON_AddNumberToObject(json, "consumptionLimit", evse_get_consumption_limit());
    cJSON_AddNumberToObject(json, "defaultConsumptionLimit", evse_get_default_consumption_limit());
    cJSON_AddNumberToObject(json, "chargingTimeLimit", evse_get_charging_time_limit());
    cJSON_AddNumberToObject(json, "defaultChargingTimeLimit", evse_get_default_charging_time_limit());
    cJSON_AddNumberToObject(json, "underPowerLimit", evse_get_under_power_limit());
    cJSON_AddNumberToObject(json, "defaultUnderPowerLimit", evse_get_default_under_power_limit());

    cJSON_AddNumberToObject(json, "socketLockOperatingTime", socket_lock_get_operating_time());
    cJSON_AddNumberToObject(json, "socketLockBreakTime", socket_lock_get_break_time());
    cJSON_AddBoolToObject(json, "socketLockDetectionHigh", socket_lock_is_detection_high());
    cJSON_AddNumberToObject(json, "socketLockRetryCount", socket_lock_get_retry_count());

    cJSON_AddStringToObject(json, "energyMeterMode", energy_meter_mode_to_str(energy_meter_get_mode()));
    cJSON_AddNumberToObject(json, "energyMeterAcVoltage", energy_meter_get_ac_voltage());
    cJSON_AddNumberToObject(json, "energyMeterThreePhases", energy_meter_is_three_phases());

    return json;
}

esp_err_t http_json_set_evse_config(cJSON* json)
{
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "maxChargingCurrent"))) {
        RETURN_ON_ERROR(evse_set_max_charging_current(cJSON_GetObjectItem(json, "maxChargingCurrent")->valuedouble));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "chargingCurrent"))) {
        RETURN_ON_ERROR(evse_set_charging_current(cJSON_GetObjectItem(json, "chargingCurrent")->valuedouble * 10));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "defaultChargingCurrent"))) {
        RETURN_ON_ERROR(evse_set_default_charging_current(cJSON_GetObjectItem(json, "defaultChargingCurrent")->valuedouble * 10));
    }
    if (cJSON_IsBool(cJSON_GetObjectItem(json, "requireAuth"))) {
        evse_set_require_auth(cJSON_IsTrue(cJSON_GetObjectItem(json, "requireAuth")));
    }
    if (cJSON_IsBool(cJSON_GetObjectItem(json, "socketOutlet"))) {
        RETURN_ON_ERROR(evse_set_socket_outlet(cJSON_IsTrue(cJSON_GetObjectItem(json, "socketOutlet"))));
    }
    if (cJSON_IsBool(cJSON_GetObjectItem(json, "rcm"))) {
        RETURN_ON_ERROR(evse_set_rcm(cJSON_IsTrue(cJSON_GetObjectItem(json, "rcm"))));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "temperatureThreshold"))) {
        RETURN_ON_ERROR(evse_set_temp_threshold(cJSON_GetObjectItem(json, "temperatureThreshold")->valuedouble));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "consumptionLimit"))) {
        evse_set_consumption_limit(cJSON_GetObjectItem(json, "consumptionLimit")->valuedouble);
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "defaultConsumptionLimit"))) {
        evse_set_default_consumption_limit(cJSON_GetObjectItem(json, "defaultConsumptionLimit")->valuedouble);
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "chargingTimeLimit"))) {
        evse_set_charging_time_limit(cJSON_GetObjectItem(json, "chargingTimeLimit")->valuedouble);
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "defaultChargingTimeLimit"))) {
        evse_set_default_charging_time_limit(cJSON_GetObjectItem(json, "defaultChargingTimeLimit")->valuedouble);
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "underPowerLimit"))) {
        evse_set_under_power_limit(cJSON_GetObjectItem(json, "underPowerLimit")->valuedouble);
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "defaultUnderPowerLimit"))) {
        evse_set_default_under_power_limit(cJSON_GetObjectItem(json, "defaultUnderPowerLimit")->valuedouble);
    }

    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "socketLockOperatingTime"))) {
        RETURN_ON_ERROR(socket_lock_set_operating_time(cJSON_GetObjectItem(json, "socketLockOperatingTime")->valuedouble));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "socketLockBreakTime"))) {
        RETURN_ON_ERROR(socket_lock_set_break_time(cJSON_GetObjectItem(json, "socketLockBreakTime")->valuedouble));
    }
    if (cJSON_IsBool(cJSON_GetObjectItem(json, "socketLockDetectionHigh"))) {
        socket_lock_set_detection_high(cJSON_IsTrue(cJSON_GetObjectItem(json, "socketLockDetectionHigh")));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "socketLockRetryCount"))) {
        socket_lock_set_retry_count(cJSON_GetObjectItem(json, "socketLockRetryCount")->valuedouble);
    }

    if (cJSON_IsString(cJSON_GetObjectItem(json, "energyMeterMode"))) {
        RETURN_ON_ERROR(energy_meter_set_mode(energy_meter_str_to_mode(cJSON_GetObjectItem(json, "energyMeterMode")->valuestring)));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(json, "energyMeterAcVoltage"))) {
        RETURN_ON_ERROR(energy_meter_set_ac_voltage(cJSON_GetObjectItem(json, "energyMeterAcVoltage")->valuedouble));
    }
    if (cJSON_IsBool(cJSON_GetObjectItem(json, "energyMeterThreePhases"))) {
        energy_meter_set_three_phases(cJSON_IsTrue(cJSON_GetObjectItem(json, "energyMeterThreePhases")));
    }

    return ESP_OK;
}

cJSON* http_json_get_wifi_config(void)
{
    cJSON* json = cJSON_CreateObject();

    char str[32];

    cJSON_AddBoolToObject(json, "enabled", wifi_get_enabled());
    wifi_get_ssid(str);
    cJSON_AddStringToObject(json, "ssid", str);

    return json;
}

static void wifi_set_config_func(void* arg)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    wifi_set_config_arg_t* config = (wifi_set_config_arg_t*)arg;
    wifi_set_config(config->enabled, config->ssid_blank ? NULL : config->ssid, config->password_blank ? NULL : config->password);
    free((void*)config);

    vTaskDelete(NULL);
}

static esp_err_t timeout_wifi_set_config(bool enabled, const char* ssid, const char* password)
{
    if (enabled) {
        if (ssid == NULL || strlen(ssid) == 0) {
            char old_ssid[32];
            wifi_get_ssid(old_ssid);
            if (strlen(old_ssid) == 0) {
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    wifi_set_config_arg_t* config = (wifi_set_config_arg_t*)malloc(sizeof(wifi_set_config_arg_t));
    config->enabled = enabled;
    if (ssid == NULL || ssid[0] == '\0') {
        config->ssid_blank = true;
    } else {
        config->ssid_blank = false;
        strcpy(config->ssid, ssid);
    }
    if (password == NULL || password[0] == '\0') {
        config->password_blank = true;
    } else {
        config->password_blank = false;
        strcpy(config->password, password);
    }

    xTaskCreate(wifi_set_config_func, "wifi_set_config", 4 * 1024, (void*)config, 10, NULL);

    return ESP_OK;
}

esp_err_t http_json_set_wifi_config(cJSON* json, bool timeout)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));
    char* ssid = cJSON_GetStringValue(cJSON_GetObjectItem(json, "ssid"));
    char* password = cJSON_GetStringValue(cJSON_GetObjectItem(json, "password"));

    if (timeout) {
        return timeout_wifi_set_config(enabled, ssid, password);
    } else {
        return wifi_set_config(enabled, ssid, password);
    }
}

cJSON* http_json_get_wifi_scan(void)
{
    cJSON* json = cJSON_CreateArray();

    wifi_scan_ap_t scan_aps[WIFI_SCAN_SCAN_LIST_SIZE];
    uint16_t number = wifi_scan(scan_aps);
    for (int i = 0; i < number; i++) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", scan_aps[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", scan_aps[i].rssi);
        cJSON_AddBoolToObject(item, "auth", scan_aps[i].auth);
        cJSON_AddItemToArray(json, item);
    }

    return json;
}

cJSON* http_json_get_serial_config(void)
{
    cJSON* json = cJSON_CreateArray();

    for (int i = 0; i < BOARD_CFG_SERIAL_COUNT; i++) {
        if (board_config.serials[i].type != BOARD_CFG_SERIAL_TYPE_NONE) {
            cJSON* serial_json = cJSON_CreateObject();
            cJSON_AddStringToObject(serial_json, "mode", serial_mode_to_str(serial_get_mode(i)));
            cJSON_AddNumberToObject(serial_json, "baudRate", serial_get_baud_rate(i));
            cJSON_AddStringToObject(serial_json, "dataBits", serial_data_bits_to_str(serial_get_data_bits(i)));
            cJSON_AddStringToObject(serial_json, "stopBits", serial_stop_bits_to_str(serial_get_stop_bits(i)));
            cJSON_AddStringToObject(serial_json, "parity", serial_parity_to_str(serial_get_parity(i)));
            cJSON_AddItemToArray(json, serial_json);
        }
    }

    return json;
}

esp_err_t http_json_set_serial_config(cJSON* json)
{
    serial_reset_config();

    cJSON* serial_json = json->child;

    for (int i = 0; i < BOARD_CFG_SERIAL_COUNT; i++) {
        if (board_config.serials[i].type != BOARD_CFG_SERIAL_TYPE_NONE) {
            if (!serial_json) return ESP_ERR_INVALID_SIZE;

            serial_mode_t mode = serial_str_to_mode(cJSON_GetObjectItem(serial_json, "mode")->valuestring);
            int baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(serial_json, "baudRate"));
            uart_word_length_t data_bits = serial_str_to_data_bits(cJSON_GetStringValue(cJSON_GetObjectItem(serial_json, "dataBits")));
            uart_word_length_t stop_bits = serial_str_to_stop_bits(cJSON_GetStringValue(cJSON_GetObjectItem(serial_json, "stopBits")));
            uart_parity_t parity = serial_str_to_parity(cJSON_GetStringValue(cJSON_GetObjectItem(serial_json, "parity")));

            RETURN_ON_ERROR(serial_set_config(i, mode, baud_rate, data_bits, stop_bits, parity));

            serial_json = serial_json->next;
        }
    }

    if (serial_json) return ESP_ERR_INVALID_SIZE;  // should no item left in array

    return ESP_OK;
}

cJSON* http_json_get_modbus_config(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddBoolToObject(json, "tcpEnabled", modbus_tcp_is_enabled());
    cJSON_AddNumberToObject(json, "unitId", modbus_get_unit_id());

    return json;
}

esp_err_t http_json_set_modbus_config(cJSON* json)
{
    bool tcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "tcpEnabled"));
    uint8_t unit_id = cJSON_GetObjectItem(json, "unitId")->valuedouble;

    modbus_tcp_set_enabled(tcp_enabled);
    return modbus_set_unit_id(unit_id);
}

cJSON* http_json_get_script_config(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddBoolToObject(json, "enabled", script_is_enabled());
    cJSON_AddBoolToObject(json, "autoReload", script_is_auto_reload());

    return json;
}

esp_err_t http_json_set_script_config(cJSON* json)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));
    bool auto_reload = cJSON_IsTrue(cJSON_GetObjectItem(json, "autoReload"));

    script_set_enabled(enabled);
    script_set_auto_reload(auto_reload);

    return ESP_OK;
}

cJSON* http_json_get_script_components(void)
{
    cJSON* json = cJSON_CreateArray();

    script_component_list_t* component_list = script_get_components();
    if (component_list) {
        script_component_entry_t* component;
        SLIST_FOREACH (component, component_list, entries) {
            cJSON* component_json = cJSON_CreateObject();
            cJSON_AddStringToObject(component_json, "id", component->id);
            cJSON_AddStringToObject(component_json, "name", component->name);
            cJSON_AddStringToObject(component_json, "description", component->description);
            cJSON_AddItemToArray(json, component_json);
        }
    }

    return json;
}

cJSON* http_json_get_script_component_config(const char* id)
{
    cJSON* json;

    script_component_param_list_t* param_list = script_get_component_params(id);
    if (param_list) {
        json = cJSON_CreateObject();
        cJSON* params_json = cJSON_CreateArray();
        script_component_param_entry_t* param;
        SLIST_FOREACH (param, param_list, entries) {
            cJSON* param_json = cJSON_CreateObject();
            cJSON_AddStringToObject(param_json, "key", param->key);
            cJSON_AddStringToObject(param_json, "name", param->name);
            cJSON_AddStringToObject(param_json, "type", script_component_param_type_to_str(param->type));
            switch (param->type) {
            case SCRIPT_COMPONENT_PARAM_TYPE_STRING:
                cJSON_AddStringToObject(param_json, "value", param->value.string);
                break;
            case SCRIPT_COMPONENT_PARAM_TYPE_NUMBER:
                cJSON_AddNumberToObject(param_json, "value", param->value.number);
                break;
            case SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN:
                cJSON_AddBoolToObject(param_json, "value", param->value.boolean);
                break;
            default:
                break;
            }
            cJSON_AddItemToArray(params_json, param_json);
        }
        cJSON_AddItemToObject(json, "params", params_json);

        script_component_params_free(param_list);
    } else {
        json = cJSON_CreateNull();
    }

    return json;
}

esp_err_t http_json_set_script_component_config(const char* id, cJSON* json)
{
    cJSON* params_json = cJSON_GetObjectItem(json, "params");
    if (cJSON_IsArray(params_json)) {
        script_component_param_list_t* param_list = (script_component_param_list_t*)malloc(sizeof(script_component_param_list_t));
        SLIST_INIT(param_list);

        cJSON* param_json = NULL;
        cJSON_ArrayForEach (param_json, params_json) {
            script_component_param_entry_t* param = (script_component_param_entry_t*)malloc(sizeof(script_component_param_entry_t));
            param->key = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(param_json, "key")));
            param->name = NULL;
            cJSON* value_json = cJSON_GetObjectItem(param_json, "value");
            if (cJSON_IsString(value_json)) {
                param->type = SCRIPT_COMPONENT_PARAM_TYPE_STRING;
                param->value.string = strdup(value_json->valuestring);
            } else if (cJSON_IsNumber(value_json)) {
                param->type = SCRIPT_COMPONENT_PARAM_TYPE_NUMBER;
                param->value.number = value_json->valuedouble;
            } else if (cJSON_IsBool(value_json)) {
                param->type = SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN;
                param->value.boolean = cJSON_IsTrue(value_json);
            } else {
                param->type = SCRIPT_COMPONENT_PARAM_TYPE_NONE;
            }

            SLIST_INSERT_HEAD(param_list, param, entries);
        }

        script_set_component_params(id, param_list);
        script_component_params_free(param_list);
    }

    return ESP_OK;
}

cJSON* http_json_get_scheduler_config(void)
{
    cJSON* json = cJSON_CreateObject();
    char str[64];

    cJSON_AddBoolToObject(json, "ntpEnabled", scheduler_is_ntp_enabled());
    scheduler_get_ntp_server(str);
    cJSON_AddStringToObject(json, "ntpServer", str);
    cJSON_AddBoolToObject(json, "ntpFromDhcp", scheduler_is_ntp_from_dhcp());
    scheduler_get_timezone(str);
    cJSON_AddStringToObject(json, "timezone", str);

    cJSON* schedules_json = cJSON_CreateArray();
    uint8_t schedule_count = scheduler_get_schedule_count();
    scheduler_schedule_t* schedules = scheduler_get_schedules();
    for (uint8_t i = 0; i < schedule_count; i++) {
        cJSON* schedule_json = cJSON_CreateObject();
        cJSON_AddStringToObject(schedule_json, "action", scheduler_action_to_str(schedules[i].action));

        cJSON_AddNumberToObject(schedule_json, "mon", schedules[i].days.week.mon);
        cJSON_AddNumberToObject(schedule_json, "tue", schedules[i].days.week.tue);
        cJSON_AddNumberToObject(schedule_json, "wed", schedules[i].days.week.wed);
        cJSON_AddNumberToObject(schedule_json, "thu", schedules[i].days.week.thu);
        cJSON_AddNumberToObject(schedule_json, "fri", schedules[i].days.week.fri);
        cJSON_AddNumberToObject(schedule_json, "sat", schedules[i].days.week.sat);
        cJSON_AddNumberToObject(schedule_json, "sun", schedules[i].days.week.sun);

        cJSON_AddItemToArray(schedules_json, schedule_json);
    }
    cJSON_AddItemToObject(json, "schedules", schedules_json);

    return json;
}

esp_err_t http_json_set_scheduler_config(cJSON* json)
{
    char* ntp_server = cJSON_GetStringValue(cJSON_GetObjectItem(json, "ntpServer"));
    bool ntp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "ntpEnabled"));
    bool ntp_from_dhcp = cJSON_IsTrue(cJSON_GetObjectItem(json, "ntpFromDhcp"));
    char* timezone = cJSON_GetStringValue(cJSON_GetObjectItem(json, "timezone"));

    RETURN_ON_ERROR(scheduler_set_ntp_config(ntp_enabled, ntp_server, ntp_from_dhcp));

    cJSON* schedules_json = cJSON_GetObjectItem(json, "schedules");
    if (cJSON_IsArray(schedules_json)) {
        uint8_t count = cJSON_GetArraySize(schedules_json);
        scheduler_schedule_t* schedules = (scheduler_schedule_t*)malloc(sizeof(scheduler_schedule_t) * count);

        uint8_t i = 0;
        cJSON* schedule_json = NULL;
        cJSON_ArrayForEach (schedule_json, schedules_json) {
            schedules[i].action = scheduler_str_to_action(cJSON_GetStringValue(cJSON_GetObjectItem(schedule_json, "action")));
            schedules[i].days.week.mon = cJSON_GetNumberValue(cJSON_GetObjectItem(schedule_json, "mon"));
            schedules[i].days.week.tue = cJSON_GetNumberValue(cJSON_GetObjectItem(schedule_json, "tue"));
            schedules[i].days.week.wed = cJSON_GetNumberValue(cJSON_GetObjectItem(schedule_json, "wed"));
            schedules[i].days.week.thu = cJSON_GetNumberValue(cJSON_GetObjectItem(schedule_json, "thu"));
            schedules[i].days.week.fri = cJSON_GetNumberValue(cJSON_GetObjectItem(schedule_json, "fri"));
            schedules[i].days.week.sat = cJSON_GetNumberValue(cJSON_GetObjectItem(schedule_json, "sat"));
            schedules[i].days.week.sun = cJSON_GetNumberValue(cJSON_GetObjectItem(schedule_json, "sun"));
            i++;
        };

        scheduler_set_schedule_config(schedules, count);
        free((void*)schedules);
    }

    RETURN_ON_ERROR(scheduler_set_timezone(timezone));

    return ESP_OK;
}

cJSON* http_json_get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return cJSON_CreateNumber(tv.tv_sec);
}

esp_err_t http_json_set_time(cJSON* json)
{
    if (cJSON_IsNumber(json)) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        tv.tv_sec = cJSON_GetNumberValue(json);
        settimeofday(&tv, NULL);

        scheduler_execute_schedules();
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

cJSON* http_json_get_state(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "state", evse_state_to_str(evse_get_state()));
    cJSON_AddBoolToObject(json, "available", evse_is_available());
    cJSON_AddBoolToObject(json, "enabled", evse_is_enabled());
    cJSON_AddBoolToObject(json, "pendingAuth", evse_is_pending_auth());
    cJSON_AddBoolToObject(json, "limitReached", evse_is_limit_reached());

    uint32_t error = evse_get_error();
    if (error == 0) {
        cJSON_AddNullToObject(json, "errors");
    } else {
        cJSON* errors_json = cJSON_CreateArray();
        if (error & EVSE_ERR_PILOT_FAULT_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("pilot_fault"));
        }
        if (error & EVSE_ERR_DIODE_SHORT_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("diode_short"));
        }
        if (error & EVSE_ERR_LOCK_FAULT_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("lock_fault"));
        }
        if (error & EVSE_ERR_UNLOCK_FAULT_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("unlock_fault"));
        }
        if (error & EVSE_ERR_RCM_TRIGGERED_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("rcm_triggered"));
        }
        if (error & EVSE_ERR_RCM_SELFTEST_FAULT_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("rcm_selftest_fault"));
        }
        if (error & EVSE_ERR_TEMPERATURE_HIGH_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("temperature_high"));
        }
        if (error & EVSE_ERR_TEMPERATURE_FAULT_BIT) {
            cJSON_AddItemToArray(errors_json, cJSON_CreateString("temperature_fault"));
        }
        cJSON_AddItemToObject(json, "errors", errors_json);
    }

    cJSON_AddNumberToObject(json, "sessionTime", energy_meter_get_session_time());
    cJSON_AddNumberToObject(json, "chargingTime", energy_meter_get_charging_time());
    cJSON_AddNumberToObject(json, "consumption", energy_meter_get_consumption());
    cJSON_AddNumberToObject(json, "power", energy_meter_get_power());
    float values[3];
    energy_meter_get_voltage(values);
    cJSON_AddItemToObject(json, "voltage", cJSON_CreateFloatArray(values, 3));
    energy_meter_get_current(values);
    cJSON_AddItemToObject(json, "current", cJSON_CreateFloatArray(values, 3));

    return json;
}

cJSON* http_json_get_info(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "uptime", esp_timer_get_time() / 1000000);
    const esp_app_desc_t* app_desc = esp_app_get_description();
    cJSON_AddStringToObject(json, "appVersion", app_desc->version);
    cJSON_AddStringToObject(json, "appDate", app_desc->date);
    cJSON_AddStringToObject(json, "appTime", app_desc->time);
    cJSON_AddStringToObject(json, "idfVersion", app_desc->idf_ver);
    cJSON_AddStringToObject(json, "chip", CONFIG_IDF_TARGET);
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(json, "chipCores", chip_info.cores);
    chip_info.revision = 301;
    cJSON_AddNumberToObject(json, "chipRevision", chip_info.revision / 100);
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL);
    cJSON_AddNumberToObject(json, "heapSize", heap_info.total_allocated_bytes);
    cJSON_AddNumberToObject(json, "maxHeapSize", heap_info.total_free_bytes + heap_info.total_allocated_bytes);
    char str[24];
    wifi_get_mac(false, str);
    cJSON_AddStringToObject(json, "mac", str);
    wifi_get_ip(false, str);
    cJSON_AddStringToObject(json, "ip", str);
    wifi_get_mac(true, str);
    cJSON_AddStringToObject(json, "macAp", str);
    wifi_get_ip(true, str);
    cJSON_AddStringToObject(json, "ipAp", str);
    cJSON_AddNumberToObject(json, "temperatureSensorCount", temp_sensor_get_count());
    cJSON_AddNumberToObject(json, "temperatureLow", temp_sensor_get_low() / 100.0);
    cJSON_AddNumberToObject(json, "temperatureHigh", temp_sensor_get_high() / 100.0);
    return json;
}

static const char* serial_type_to_str(board_cfg_serial_type_t type)
{
    switch (type) {
    case BOARD_CFG_SERIAL_TYPE_UART:
        return "uart";
    case BOARD_CFG_SERIAL_TYPE_RS485:
        return "rs485";
    default:
        return "none";
    }
}

cJSON* http_json_get_board_config(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "deviceName", board_config.device_name);
    cJSON_AddBoolToObject(json, "socketLock", board_cfg_is_socket_lock(board_config));
    cJSON_AddBoolToObject(json, "proximity", board_cfg_is_proximity(board_config));
    cJSON_AddNumberToObject(json, "socketLockMinBreakTime", board_config.socket_lock_min_break_time);
    cJSON_AddBoolToObject(json, "rcm", board_cfg_is_rcm(board_config));
    cJSON_AddBoolToObject(json, "temperatureSensor", board_cfg_is_onewire(board_config) && board_config.onewire_temp_sensor);

    const char* energy_meter = "none";
    bool energy_meter_three_phases = false;
    if (board_cfg_is_energy_meter_cur(board_config)) {
        if (board_cfg_is_energy_meter_vlt(board_config)) {
            energy_meter = "cur_vlt";
            energy_meter_three_phases = board_cfg_is_energy_meter_vlt_3p(board_config) && board_cfg_is_energy_meter_vlt_3p(board_config);
        } else {
            energy_meter = "cur";
            energy_meter_three_phases = board_cfg_is_energy_meter_vlt_3p(board_config);
        }
    }
    cJSON_AddStringToObject(json, "energyMeter", energy_meter);
    cJSON_AddBoolToObject(json, "energyMeterThreePhases", energy_meter_three_phases);

    cJSON* array_json = cJSON_CreateArray();
    for (int i = 0; i < BOARD_CFG_SERIAL_COUNT; i++) {
        if (board_config.serials[i].type != BOARD_CFG_SERIAL_TYPE_NONE) {
            cJSON* serial_json = cJSON_CreateObject();
            cJSON_AddStringToObject(serial_json, "type", serial_type_to_str(board_config.serials[i].type));
            cJSON_AddStringToObject(serial_json, "name", board_config.serials[i].name);
            cJSON_AddItemToArray(array_json, serial_json);
        }
    }
    cJSON_AddItemToObject(json, "serials", array_json);

    array_json = cJSON_CreateArray();
    for (int i = 0; i < BOARD_CFG_AUX_INPUT_COUNT; i++) {
        if (board_cfg_is_aux_input(board_config, i)) {
            cJSON_AddItemToArray(array_json, cJSON_CreateString(board_config.aux_inputs[i].name));
        }
    }
    cJSON_AddItemToObject(json, "auxInputs", array_json);

    array_json = cJSON_CreateArray();
    for (int i = 0; i < BOARD_CFG_AUX_OUTPUT_COUNT; i++) {
        if (board_cfg_is_aux_output(board_config, i)) {
            cJSON_AddItemToArray(array_json, cJSON_CreateString(board_config.aux_outputs[i].name));
        }
    }
    cJSON_AddItemToObject(json, "auxOutputs", array_json);

    array_json = cJSON_CreateArray();
    for (int i = 0; i < BOARD_CFG_AUX_ANALOG_INPUT_COUNT; i++) {
        if (board_cfg_is_aux_analog_input(board_config, i)) {
            cJSON_AddItemToArray(array_json, cJSON_CreateString(board_config.aux_analog_inputs[i].name));
        }
    }
    cJSON_AddItemToObject(json, "auxAnalogInputs", array_json);

    return json;
}
