#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "esp_err.h"
#include "cJSON.h"

cJSON* json_get_evse_config(void);

esp_err_t json_set_evse_config(cJSON* root);

cJSON* json_get_wifi_config(void);

esp_err_t json_set_wifi_config(cJSON* root, bool timeout);

cJSON* json_get_wifi_scan(void);

cJSON* json_get_mqtt_config(void);

esp_err_t json_set_mqtt_config(cJSON* root);

cJSON* json_get_serial_config(void);

esp_err_t json_set_serial_config(cJSON* root);

cJSON* json_get_modbus_config(void);

esp_err_t json_set_modbus_config(cJSON* root);

cJSON* json_get_script_config(void);

esp_err_t json_set_script_config(cJSON* root);

cJSON* json_get_state(void);

cJSON* json_get_info(void);

cJSON* json_get_board_config(void);

#endif /* JSON_UTILS_H */
