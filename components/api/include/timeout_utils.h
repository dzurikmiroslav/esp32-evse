#ifndef TIMEOUT_UTILS_H
#define TIMEOUT_UTILS_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Restart in 5 seconds
 * 
 */
void timeout_restart(void);

/**
 * @brief Set WiFi config in 1 second
 * 
 * @param enabled 
 * @param ssid 
 * @param password 
 * @return esp_err_t 
 */
esp_err_t timeout_wifi_set_config(bool enabled, const char* ssid, const char* password);

#endif /* TIMEOUT_UTILS_H */
