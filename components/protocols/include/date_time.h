#ifndef DATE_TIME_H
#define DATE_TIME_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize timezone and NTP
 * 
 */
void date_time_init(void);

/**
 * @brief Return true if NTP is enabled, stored in NVS
 * 
 * @return true 
 * @return false 
 */
bool date_time_is_ntp_enabled(void);

/**
 * @brief Get NTP server, string length 64, stored in NVS
 *
 * @param value
 */
void date_time_get_ntp_server(char* value);

/**
 * @brief  Return true if NTP server from DHCP fetch is enabled, stored in NVS
 * 
 * @return true 
 * @return false 
 */
bool date_time_is_ntp_from_dhcp(void);

/**
 * @brief Set timezone and NTP config
 * 
 * @param enabled 
 * @param server 
 * @param from_dhcp 
 * @return esp_err_t 
 */
esp_err_t date_time_set_ntp_config(bool enabled, const char* server, bool from_dhcp);

/**
 * @brief Get timezone, string length 64, stored in NVS
 *
 * @param value
 */
void date_time_get_timezone(char* value);

/**
 * @brief Set timezone, string length 64, stored in NVS
 *
 * @param value
 */
esp_err_t date_time_set_timezone(const char* value);


#endif /* DATE_TIME_H */