#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "cJSON.h"

cJSON* json_get_evse_config(void);

void json_set_evse_config(cJSON* root);

cJSON* json_get_wifi_config(void);

void json_set_wifi_config(cJSON* root);

cJSON* json_get_mqtt_config(void);

void json_set_mqtt_config(cJSON* root);

cJSON* json_get_tcp_logger_config(void);

void json_set_tcp_logger_config(cJSON* root);

cJSON* json_get_energy_meter_config(void);

void json_set_energy_meter_config(cJSON* root);

cJSON* json_get_state(void);

cJSON* json_get_info(void);

cJSON* json_get_board_config(void);

#endif /* JSON_UTILS_H */
