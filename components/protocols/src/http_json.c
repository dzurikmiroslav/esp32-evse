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
    cJSON* json = cJSON_CreateObject();

    char mode_name[16];
    char baud_rate_name[16];
    char data_bits_name[16];
    char stop_bits_name[16];
    char parity_name[16];

    for (serial_id_t id = 0; id < SERIAL_ID_MAX; id++) {
        if (serial_is_available(id)) {
            sprintf(mode_name, "serial%dMode", id + 1);
            sprintf(baud_rate_name, "serial%dBaudRate", id + 1);
            sprintf(data_bits_name, "serial%dDataBits", id + 1);
            sprintf(stop_bits_name, "serial%dStopBits", id + 1);
            sprintf(parity_name, "serial%dParity", id + 1);

            cJSON_AddStringToObject(json, mode_name, serial_mode_to_str(serial_get_mode(id)));
            cJSON_AddNumberToObject(json, baud_rate_name, serial_get_baud_rate(id));
            cJSON_AddStringToObject(json, data_bits_name, serial_data_bits_to_str(serial_get_data_bits(id)));
            cJSON_AddStringToObject(json, stop_bits_name, serial_stop_bits_to_str(serial_get_stop_bits(id)));
            cJSON_AddStringToObject(json, parity_name, serial_parity_to_str(serial_get_parity(id)));
        }
    }

    return json;
}

esp_err_t http_json_set_serial_config(cJSON* json)
{
    char mode_name[16];
    char baud_rate_name[16];
    char data_bits_name[16];
    char stop_bits_name[16];
    char parity_name[16];
    serial_reset_config();

    for (serial_id_t id = 0; id < SERIAL_ID_MAX; id++) {
        if (serial_is_available(id)) {
            sprintf(mode_name, "serial%dMode", id + 1);
            sprintf(baud_rate_name, "serial%dBaudRate", id + 1);
            sprintf(data_bits_name, "serial%dDataBits", id + 1);
            sprintf(stop_bits_name, "serial%dStopBits", id + 1);
            sprintf(parity_name, "serial%dParity", id + 1);

            if (cJSON_IsString(cJSON_GetObjectItem(json, mode_name))) {
                serial_mode_t mode = serial_str_to_mode(cJSON_GetObjectItem(json, mode_name)->valuestring);
                int baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(json, baud_rate_name));
                uart_word_length_t data_bits = serial_str_to_data_bits(cJSON_GetStringValue(cJSON_GetObjectItem(json, data_bits_name)));
                uart_word_length_t stop_bits = serial_str_to_stop_bits(cJSON_GetStringValue(cJSON_GetObjectItem(json, stop_bits_name)));
                uart_parity_t parity = serial_str_to_parity(cJSON_GetStringValue(cJSON_GetObjectItem(json, parity_name)));

                RETURN_ON_ERROR(serial_set_config(id, mode, baud_rate, data_bits, stop_bits, parity));
            }
        }
    }

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

    return json;
}

esp_err_t http_json_set_script_config(cJSON* json)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(json, "enabled"));

    script_set_enabled(enabled);

    return ESP_OK;
}

cJSON* http_json_get_script_driver_config(uint8_t index)
{
    cJSON* json = cJSON_CreateObject();

    script_driver_t* driver = script_driver_get(index);
    if (driver != NULL) {
        cJSON_AddStringToObject(json, "name", driver->name);
        cJSON_AddStringToObject(json, "description", driver->description);

        cJSON* cfg_entries_json = cJSON_CreateArray();
        for (uint8_t i = 0; i < driver->cfg_entries_count; i++) {
            script_driver_cfg_meta_entry_t* cfg_entry = &driver->cfg_entries[i];
            if (cfg_entry->type != SCRIPT_DRIVER_CFG_ENTRY_TYPE_NONE) {
                cJSON* cfg_entry_json = cJSON_CreateObject();
                cJSON_AddStringToObject(cfg_entry_json, "key", cfg_entry->key);
                cJSON_AddStringToObject(cfg_entry_json, "name", cfg_entry->name);
                cJSON_AddStringToObject(cfg_entry_json, "type", script_driver_cfg_entry_type_to_str(cfg_entry->type));
                switch (cfg_entry->type) {
                case SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING:
                    cJSON_AddStringToObject(cfg_entry_json, "value", cfg_entry->value.string);
                    break;
                case SCRIPT_DRIVER_CFG_ENTRY_TYPE_NUMBER:
                    cJSON_AddNumberToObject(cfg_entry_json, "value", cfg_entry->value.number);
                    break;
                case SCRIPT_DRIVER_CFG_ENTRY_TYPE_BOOLEAN:
                    cJSON_AddBoolToObject(cfg_entry_json, "value", cfg_entry->value.boolean);
                    break;
                default:
                    break;
                }
                cJSON_AddItemToArray(cfg_entries_json, cfg_entry_json);
            }
        }
        cJSON_AddItemToObject(json, "config", cfg_entries_json);

        script_driver_free(driver);
    }

    return json;
}

cJSON* http_json_get_script_drivers_config(void)
{
    cJSON* json = cJSON_CreateArray();

    uint8_t count = script_driver_get_count();
    for (uint8_t i = 0; i < count; i++) {
        cJSON_AddItemToArray(json, http_json_get_script_driver_config(i));
    }

    return json;
}

esp_err_t http_json_set_script_driver_config(uint8_t index, cJSON* json)
{
    cJSON* config_json = cJSON_GetObjectItem(json, "config");
    if (cJSON_IsArray(config_json)) {
        uint8_t cfg_entries_count = cJSON_GetArraySize(config_json);
        script_driver_cfg_entry_t* cfg_entries = (script_driver_cfg_entry_t*)malloc(sizeof(script_driver_cfg_entry_t) * cfg_entries_count);

        uint8_t i = 0;
        cJSON* entry_json = NULL;
        cJSON_ArrayForEach (entry_json, config_json) {
            script_driver_cfg_entry_t* cfg_entry = &cfg_entries[i];
            cfg_entry->key = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(entry_json, "key")));
            cJSON* value_json = cJSON_GetObjectItem(entry_json, "value");
            if (cJSON_IsString(value_json)) {
                cfg_entry->type = SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING;
                cfg_entry->value.string = strdup(value_json->valuestring);
            } else if (cJSON_IsNumber(value_json)) {
                cfg_entry->type = SCRIPT_DRIVER_CFG_ENTRY_TYPE_NUMBER;
                cfg_entry->value.number = value_json->valuedouble;
            } else if (cJSON_IsBool(value_json)) {
                cfg_entry->type = SCRIPT_DRIVER_CFG_ENTRY_TYPE_BOOLEAN;
                cfg_entry->value.boolean = cJSON_IsTrue(value_json);
            } else {
                cfg_entry->type = SCRIPT_DRIVER_CFG_ENTRY_TYPE_NONE;
            }

            i++;
        };

        script_driver_set_config(index, cfg_entries, cfg_entries_count);
        script_driver_cfg_entries_free(cfg_entries, cfg_entries_count);
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
    uint8_t mac[6];
    char str[32];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    sprintf(str, MACSTR, MAC2STR(mac));
    cJSON_AddStringToObject(json, "mac", str);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, str, sizeof(str));
    cJSON_AddStringToObject(json, "ip", str);
    esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
    sprintf(str, MACSTR, MAC2STR(mac));
    cJSON_AddStringToObject(json, "macAp", str);
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, str, sizeof(str));
    cJSON_AddStringToObject(json, "ipAp", str);
    cJSON_AddNumberToObject(json, "temperatureSensorCount", temp_sensor_get_count());
    cJSON_AddNumberToObject(json, "temperatureLow", temp_sensor_get_low() / 100.0);
    cJSON_AddNumberToObject(json, "temperatureHigh", temp_sensor_get_high() / 100.0);
    return json;
}

static const char* serial_to_str(board_config_serial_t serial)
{
    switch (serial) {
    case BOARD_CONFIG_SERIAL_UART:
        return "uart";
    case BOARD_CONFIG_SERIAL_RS485:
        return "rs485";
    default:
        return "none";
    }
}

cJSON* http_json_get_board_config(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddStringToObject(json, "deviceName", board_config.device_name);
    cJSON_AddBoolToObject(json, "socketLock", board_config.socket_lock);
    cJSON_AddBoolToObject(json, "proximity", board_config.proximity);
    cJSON_AddNumberToObject(json, "socketLockMinBreakTime", board_config.socket_lock_min_break_time);
    cJSON_AddBoolToObject(json, "rcm", board_config.rcm);
    cJSON_AddBoolToObject(json, "temperatureSensor", board_config.onewire && board_config.onewire_temp_sensor);
    switch (board_config.energy_meter) {
    case BOARD_CONFIG_ENERGY_METER_CUR:
        cJSON_AddStringToObject(json, "energyMeter", "cur");
        break;
    case BOARD_CONFIG_ENERGY_METER_CUR_VLT:
        cJSON_AddStringToObject(json, "energyMeter", "cur_vlt");
        break;
    default:
        cJSON_AddStringToObject(json, "energyMeter", "none");
    }
    cJSON_AddBoolToObject(json, "energyMeterThreePhases", board_config.energy_meter_three_phases);

    cJSON_AddStringToObject(json, "serial1", serial_to_str(board_config.serial_1));
    cJSON_AddStringToObject(json, "serial1Name", board_config.serial_1_name);
    cJSON_AddStringToObject(json, "serial2", serial_to_str(board_config.serial_2));
    cJSON_AddStringToObject(json, "serial2Name", board_config.serial_2_name);
#if SOC_UART_NUM > 2
    cJSON_AddStringToObject(json, "serial3", serial_to_str(board_config.serial_3));
    cJSON_AddStringToObject(json, "serial3Name", board_config.serial_3_name);
#else
    cJSON_AddStringToObject(json, "serial3", serial_to_str(BOARD_CONFIG_SERIAL_NONE));
    cJSON_AddStringToObject(json, "serial3Name", "");
#endif

    cJSON* aux_json = cJSON_CreateArray();
    if (board_config.aux_in_1) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_in_1_name));
    }
    if (board_config.aux_in_2) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_in_2_name));
    }
    if (board_config.aux_in_3) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_in_3_name));
    }
    if (board_config.aux_in_4) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_in_4_name));
    }
    cJSON_AddItemToObject(json, "auxIn", aux_json);

    aux_json = cJSON_CreateArray();
    if (board_config.aux_out_1) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_out_1_name));
    }
    if (board_config.aux_out_2) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_out_2_name));
    }
    if (board_config.aux_out_3) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_out_3_name));
    }
    if (board_config.aux_out_4) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_out_4_name));
    }
    cJSON_AddItemToObject(json, "auxOut", aux_json);

    aux_json = cJSON_CreateArray();
    if (board_config.aux_ain_1) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_ain_1_name));
    }
    if (board_config.aux_ain_2) {
        cJSON_AddItemToArray(aux_json, cJSON_CreateString(board_config.aux_ain_2_name));
    }
    cJSON_AddItemToObject(json, "auxAin", aux_json);

    return json;
}
