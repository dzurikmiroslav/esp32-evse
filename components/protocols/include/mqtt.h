#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize MQTT
 * 
 */
void mqtt_init(void);

/**
 * @brief Set MQTT config
 * 
 * @param enabled 
 * @param server NULL value will be skiped
 * @param base_topic NULL value will be skiped
 * @param user  NULL value will be skiped
 * @param password  NULL value will be skiped
 * @param periodicity 0 value will be skiped
 * @return esp_err_t 
 */
esp_err_t mqtt_set_config(bool enabled, const char* server, const char* base_topic, const char* user, const char* password, uint16_t periodicity);

/**
 * @brief Get MQTT enabled, stored in NVS
 * 
 * @return true 
 * @return false 
 */
bool mqtt_get_enabled(void);

/**
 * @brief Get MQTT server, string length 64, stored in NVS
 * 
 * @param value 
 */
void mqtt_get_server(char* value);

/**
 * @brief Get MQTT password, string length 64, stored in NVS
 * 
 * @param value 
 */
void mqtt_get_password(char* value); 

/**
 * @brief Get MQTT base topic, string length 32, stored in NVS
 * 
 * @param value 
 */
void mqtt_get_base_topic(char* value);

/**
 * @brief Get MQTT user, string length 32, stored in NVS
 * 
 * @param value 
 */
void mqtt_get_user(char* value); 

/**
 * @brief Get MQTT periodicity in second, stored in NVS
 * 
 * @return uint16_t 
 */
uint16_t mqtt_get_periodicity(void);

#endif /* MQTT_H_ */
