#ifndef HTTP_JSON_UTILS_H
#define HTTP_JSON_UTILS_H

#include <cJSON.h>
#include <esp_err.h>
#include <stdbool.h>

cJSON* http_json_get_config(void);

cJSON* http_json_get_config_evse(void);

esp_err_t http_json_set_config_evse(cJSON* json);

cJSON* http_json_get_config_wifi(void);

esp_err_t http_json_set_config_wifi(cJSON* json);

cJSON* http_json_get_wifi_scan(void);

cJSON* http_json_get_wifi_state(void);

esp_err_t http_json_set_wifi_state_ap(cJSON* json);

cJSON* http_json_get_config_discovery(void);

esp_err_t http_json_set_config_discovery(cJSON* json);

cJSON* http_json_get_config_serial(void);

esp_err_t http_json_set_config_serial(cJSON* json);

cJSON* http_json_get_config_modbus(void);

esp_err_t http_json_set_config_modbus(cJSON* json);

cJSON* http_json_get_config_script(void);

esp_err_t http_json_set_config_script(cJSON* json);

cJSON* http_json_get_script_components(void);

cJSON* http_json_get_script_component_config(const char* id);

esp_err_t http_json_set_script_component_config(const char* id, cJSON* json);

cJSON* http_json_get_config_scheduler(void);

esp_err_t http_json_set_config_scheduler(cJSON* json);

cJSON* http_json_get_time(void);

esp_err_t http_json_set_time(cJSON* json);

cJSON* http_json_get_state(void);

esp_err_t http_json_set_state(cJSON* json);

esp_err_t http_json_set_state_available(cJSON* json);

esp_err_t http_json_set_state_enabled(cJSON* json);

esp_err_t http_json_set_state_charging_current(cJSON* json);

esp_err_t http_json_set_state_consumption_limit(cJSON* json);

esp_err_t http_json_set_state_charging_time_limit(cJSON* json);

esp_err_t http_json_set_state_under_power_limit(cJSON* json);

cJSON* http_json_get_info(void);

cJSON* http_json_get_board_config(void);

cJSON* http_json_get_nextion_info(void);

cJSON* http_json_firmware_check_update(void);

cJSON* http_json_firmware_channel(void);

esp_err_t http_json_set_firmware_channel(cJSON* json);

cJSON* http_json_firmware_channels(void);

esp_err_t http_json_set_credentials(cJSON* root);

#endif /* HTTP_JSON_UTILS_H */
