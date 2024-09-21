#ifndef EVSE_H_
#define EVSE_H_

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#define evse_state_is_session(state)  (state >= EVSE_STATE_B1 && state <= EVSE_STATE_D2)
#define evse_state_is_charging(state) (state == EVSE_STATE_C2 || state == EVSE_STATE_D2)

#define EVSE_ERR_PILOT_FAULT_BIT        (1UL << 0)
#define EVSE_ERR_DIODE_SHORT_BIT        (1UL << 1)
#define EVSE_ERR_LOCK_FAULT_BIT         (1UL << 2)
#define EVSE_ERR_UNLOCK_FAULT_BIT       (1UL << 3)
#define EVSE_ERR_RCM_TRIGGERED_BIT      (1UL << 4)
#define EVSE_ERR_RCM_SELFTEST_FAULT_BIT (1UL << 5)
#define EVSE_ERR_TEMPERATURE_HIGH_BIT   (1UL << 6)
#define EVSE_ERR_TEMPERATURE_FAULT_BIT  (1UL << 7)

#define EVSE_ERR_AUTO_CLEAR_BITS (EVSE_ERR_PILOT_FAULT_BIT | EVSE_ERR_DIODE_SHORT_BIT | EVSE_ERR_RCM_TRIGGERED_BIT | EVSE_ERR_RCM_SELFTEST_FAULT_BIT)

/**
 * @brief States of evse controller
 *
 */
typedef enum {
    EVSE_STATE_A,
    EVSE_STATE_B1,
    EVSE_STATE_B2,
    EVSE_STATE_C1,
    EVSE_STATE_C2,
    EVSE_STATE_D1,
    EVSE_STATE_D2,
    EVSE_STATE_E,
    EVSE_STATE_F
} evse_state_t;

/**
 * @brief Initialize evse
 *
 */
void evse_init(void);

/**
 * @brief Reset evse, used only in tests
 *
 */
void evse_reset(void);

/**
 * @brief Set evse controller to available state or F
 *
 * @param available
 */
void evse_set_available(bool available);

/**
 * @brief Return true if evse controller is available
 *
 * @return true
 * @return false
 */
bool evse_is_available(void);

/**
 * @brief Main loop of evse
 *
 */
void evse_process(void);

/**
 * @brief Return current evse state
 *
 * @return evse_state_t
 */
evse_state_t evse_get_state(void);

/**
 * @brief Format to string value
 *
 * @param state
 * @return const char
 */
const char *evse_state_to_str(evse_state_t state);

/**
 * @brief Return error bits when state is EVSE_STATE_E
 *
 * @return uint32_t
 */
uint32_t evse_get_error(void);

/**
 * @brief Get max charging current, stored in NVS
 *
 * @return current in A
 */
uint8_t evse_get_max_charging_current(void);

/**
 * @brief Set max charging current, stored in NVS
 *
 * @param max_charging_current current in A
 * @return esp_err_t
 */
esp_err_t evse_set_max_charging_current(uint8_t max_charging_current);

/**
 * @brief Get charging current
 *
 * @return current in A*10
 */
uint16_t evse_get_charging_current(void);

/**
 * @brief Set charging current
 *
 * @param charging_current current in A*10
 * @return esp_err_t
 */
esp_err_t evse_set_charging_current(uint16_t charging_current);

/**
 * @brief Get default charging current, stored in NVS
 *
 * @return current in A*10
 */
uint16_t evse_get_default_charging_current(void);

/**
 * @brief Set default charging current, stored in NVS
 *
 * @param charging_current current in A*10
 * @return esp_err_t
 */
esp_err_t evse_set_default_charging_current(uint16_t charging_current);

/**
 * @brief Is required authorization to start charging, stored in NVS
 *
 * @return true
 * @return false
 */
bool evse_is_require_auth(void);

/**
 * @brief Set authorization required to start charging, stored in NVS
 *
 * @param require_auth
 */
void evse_set_require_auth(bool require_auth);

/**
 * @brief Authorize to start charging when authorization is required
 *
 */
void evse_authorize(void);

/**
 * @brief Check when is pending authorize to start charging
 *
 * @return true
 * @return false
 */
bool evse_is_pending_auth(void);

/**
 * @brief Return evse charging enabled
 *
 * @return true
 * @return false
 */
bool evse_is_enabled(void);

/**
 * @brief Set enabled charging
 *
 * @param enabled
 */
void evse_set_enabled(bool enabled);

/**
 * @brief Is session consumption, charging time or under power limit reached
 *
 * @return true
 * @return false
 */
bool evse_is_limit_reached(void);

/**
 * @brief Get consumption limit
 *
 * @return consumption in Wh
 */
uint32_t evse_get_consumption_limit(void);

/**
 * @brief Set socket outlet, stored in NVS
 *
 * @param socket_outlet
 * @return esp_err_t
 */
esp_err_t evse_set_socket_outlet(bool socket_outlet);

/**
 * @brief Get socket outlet, stored in NVS
 *
 * @return true
 * @return false
 */
bool evse_get_socket_outlet(void);

/**
 * @brief Set residual current monitoring, stored in NVS
 *
 * @param rcm
 * @return esp_err_t
 */
esp_err_t evse_set_rcm(bool rcm);

/**
 * @brief Get residual current monitoring, stored in NVS
 *
 * @return true
 * @return false
 */
bool evse_is_rcm(void);

/**
 * @brief Set temperature threshold, stored in NVS
 *
 * @param temperature in dg. C
 * @return esp_err_t
 */
esp_err_t evse_set_temp_threshold(uint8_t temp_threshold);

/**
 * @brief Get temperature threshold, stored in NVS
 *
 * @return temperature in dg. C
 */
uint8_t evse_get_temp_threshold(void);

/**
 * @brief Set consumption limit
 *
 * @param consumption_limit Consumption in Ws
 */
void evse_set_consumption_limit(uint32_t consumption_limit);

/**
 * @brief Get charging time limit
 *
 * @return Time in s
 */
uint32_t evse_get_charging_time_limit(void);

/**
 * @brief Set charging time limit
 *
 * @param charging_time_limit Time in s
 */
void evse_set_charging_time_limit(uint32_t charging_time_limit);

/**
 * @brief Get under power limit
 *
 * @return power in W
 */
uint16_t evse_get_under_power_limit(void);

/**
 * @brief Set under power limit
 *
 * @param under_power_limit power in W
 */
void evse_set_under_power_limit(uint16_t under_power_limit);

/**
 * @brief Get consumption limit, stored in NVS
 *
 * @return consumption in Ws
 */
uint32_t evse_get_default_consumption_limit(void);

/**
 * @brief Set consumption limit, stored in NVS
 *
 * @param consumption_limit consumption in 1Ws
 */
void evse_set_default_consumption_limit(uint32_t consumption_limit);

/**
 * @brief Get charging time limit, stored in NVS
 *
 * @return Time in s
 */
uint32_t evse_get_default_charging_time_limit(void);

/**
 * @brief Set charging time limit, stored in NVS
 *
 * @param charging_time_limit Time in s
 */
void evse_set_default_charging_time_limit(uint32_t charging_time_limit);

/**
 * @brief Get under power limit, stored in NVS
 *
 * @return power in W
 */
uint16_t evse_get_default_under_power_limit(void);

/**
 * @brief Set under power limit, stored in NVS
 *
 * @param under_power_limit power in W
 */
void evse_set_default_under_power_limit(uint16_t under_power_limit);

#endif /* EVSE_H_ */
