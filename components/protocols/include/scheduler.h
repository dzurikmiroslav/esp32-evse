#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Scheduler actions
 *
 */
typedef enum {
    SCHEDULER_ACTION_ENABLE,
    SCHEDULER_ACTION_AVAILABLE,
    SCHEDULER_ACTION_CHARGING_CURRENT_6A,
    SCHEDULER_ACTION_CHARGING_CURRENT_8A,
    SCHEDULER_ACTION_CHARGING_CURRENT_10A
} scheduler_action_t;

/**
 * @brief Scheduler schedule
 *
 */
typedef struct {
    scheduler_action_t action;

    union {
        struct {
            uint32_t sun;
            uint32_t mon;
            uint32_t tue;
            uint32_t wed;
            uint32_t thu;
            uint32_t fri;
            uint32_t sat;
        } week;

        uint32_t order[7];
    } days;
} scheduler_schedule_t;

/**
 * @brief Initialize scheduler
 *
 */
void scheduler_init(void);

/**
 * @brief Execute schedules after change time
 *
 */
void scheduler_execute_schedules(void);

/**
 * @brief Return number of schedulers
 *
 * @return uint8_t
 */
uint8_t scheduler_get_schedule_count(void);

/**
 * @brief Return schedulers array
 *
 * @return scheduler_schedule_t*
 */
scheduler_schedule_t* scheduler_get_schedules(void);

/**
 * @brief Set schedulers config
 *
 * @param schedules
 * @param schedule_count
 */
void scheduler_set_schedule_config(const scheduler_schedule_t* schedules, uint8_t schedule_count);

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
 * @return scheduler_task_action_t
 */
scheduler_action_t scheduler_str_to_action(const char* str);

#endif /* SCHEDULER_H */