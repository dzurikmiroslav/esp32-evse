#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include "esp_err.h"

#define SCHEDULER_ID_MAX    5

/**
 * @brief Scheduler actions
 *
 */
typedef enum
{
    SCHEDULER_ACTION_NONE,
    SCHEDULER_ACTION_ENABLE,
    SCHEDULER_ACTION_AVAILABLE,
    SCHEDULER_ACTION_CHARGING_CURRENT_6A,
    SCHEDULER_ACTION_CHARGING_CURRENT_8A,
    SCHEDULER_ACTION_CHARGING_CURRENT_10A
} scheduler_action_t;

/**
 * @brief Initialize scheduler
 *
 */
void scheduler_init(void);

/**
 * @brief Return scheduler action
 *
 * @param id up to SCHEDULER_ID_MAX
 * @return scheduler_action_t
 */
scheduler_action_t scheduler_get_action(uint8_t id);

/**
 * @brief Return scheduler days hour bit flags
 *
 * @param id
 * @return uint32_t* array with length of 7
 */
uint32_t* scheduler_get_days(uint8_t id);

/**
 * @brief Set scheduler config
 *
 * @param id up to SCHEDULER_ID_MAX
 * @param enabled
 * @param days
 * @param action
 * @return esp_err_t
 */
esp_err_t scheduler_set_config(uint8_t id, scheduler_action_t action, uint32_t* days);

/**
 * @brief Return true if NTP is enabled, stored in NVS
 *
 * @return true
 * @return false
 */
bool scheduler_is_ntp_enabled(void);

/**
 * @brief Get NTP server, string length 64, stored in NVS
 *
 * @param value
 */
void scheduler_get_ntp_server(char* value);

/**
 * @brief Return true if NTP server from DHCP fetch is enabled, stored in NVS
 *
 * @return true
 * @return false
 */
bool scheduler_is_ntp_from_dhcp(void);

/**
 * @brief Set timezone and NTP config
 *
 * @param enabled
 * @param server
 * @param from_dhcp
 * @return esp_err_t
 */
esp_err_t scheduler_set_ntp_config(bool enabled, const char* server, bool from_dhcp);

/**
 * @brief Get timezone, string length 64, stored in NVS
 *
 * @param value
 */
void scheduler_get_timezone(char* value);

/**
 * @brief Set timezone, string length 64, stored in NVS
 *
 * @param value
 */
esp_err_t scheduler_set_timezone(const char* value);

/**
 * @brief Format to string value
 * 
 * @param action 
 * @return const char* 
 */
const char* scheduler_action_to_str(scheduler_action_t action);

/**
 * @brief Parse from string value
 *
 * @param str
 * @return scheduler_action_t
 */
scheduler_action_t scheduler_str_to_action(const char* str);

#endif /* SCHEDULER_H */