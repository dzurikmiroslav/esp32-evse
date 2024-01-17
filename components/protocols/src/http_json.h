#ifndef HTTP_JSON_UTILS_H
#define HTTP_JSON_UTILS_H

#include "esp_err.h"
#include "cJSON.h"

cJSON* http_json_get_evse_config(void);

esp_err_t http_json_set_evse_config(cJSON* root);

cJSON* http_json_get_wifi_config(void);

esp_err_t http_json_set_wifi_config(cJSON* root, bool timeout);

cJSON* http_json_get_wifi_scan(void);

cJSON* http_json_get_serial_config(void);

esp_err_t http_json_set_serial_config(cJSON* root);

cJSON* http_json_get_modbus_config(void);

esp_err_t http_json_set_modbus_config(cJSON* root);

cJSON* http_json_get_script_config(void);

esp_err_t http_json_set_script_config(cJSON* root);

cJSON* http_json_get_scheduler_config(void);

esp_err_t http_json_set_scheduler_config(cJSON* root);

cJSON* http_json_get_state(void);

cJSON* http_json_get_info(void);

cJSON* http_json_get_board_config(void);

#endif /* HTTP_JSON_UTILS_H */
