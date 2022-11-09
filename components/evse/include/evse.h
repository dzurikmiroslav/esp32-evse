#ifndef EVSE_H_
#define EVSE_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define evse_state_is_session(state)        (state >= EVSE_STATE_B && state <= EVSE_STATE_D)
#define evse_state_is_charging(state)       (state >= EVSE_STATE_C && state <= EVSE_STATE_D)

/**
 * @brief States of evse controller
 *
 */
typedef enum
{
    EVSE_STATE_A,
    EVSE_STATE_B,
    EVSE_STATE_C,
    EVSE_STATE_D,
    EVSE_STATE_E,
    EVSE_STATE_F
} evse_state_t;

/**
 * @brief Error codes when state is EVSE_STATE_E
 *
 */
typedef enum
{
    EVSE_ERR_NONE,
    EVSE_ERR_PILOT_FAULT,
    EVSE_ERR_DIODE_SHORT,
    EVSE_ERR_LOCK_FAULT,
    EVSE_ERR_UNLOCK_FAULT,
    EVSE_ERR_RCM_TRIGGERED,
    EVSE_ERR_RCM_SELFTEST_FAULT
} evse_err_t;

/**
 * @brief Initialize evse
 *
 */
void evse_init(void);

/**
 * @brief Set evse controller to avalable state or F
 * 
 * @param avalable 
 */
void evse_set_avalable(bool avalable);

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
 * @brief Return detailed error code when state is EVSE_STATE_E
 *
 * @return evse_err_t
 */
evse_err_t evse_get_error(void);

/**
 * @brief Get charging current
 *
 * @return current in A*10
 */
uint16_t evse_get_chaging_current(void);

/**
 * @brief Set charging current
 *
 * @param charging_current current in A*10
 * @return esp_err_t 
 */
esp_err_t evse_set_chaging_current(uint16_t charging_current);

/**
 * @brief Get default charging current, stored in NVS
 *
 * @return current in A*10
 */
uint16_t evse_get_default_chaging_current(void);

/**
 * @brief Set default charging current, stored in NVS
 *
 * @param charging_current current in A*10
 * @return esp_err_t 
 */
esp_err_t evse_set_default_chaging_current(uint16_t charging_current);

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
 * @brief Is session consumption, elapsed or under power limit reached
 * 
 * @return true 
 * @return false 
 */
bool evse_is_limit_reached(void);

/**
 * @brief Get consumption limit
 *
 * @return consumption in Ws
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
 * @brief Set residaul current monitoring, stored in NVS
 * 
 * @param rcm 
 * @return esp_err_t 
 */
esp_err_t evse_set_rcm(bool rcm);

/**
 * @brief Get residaul current monitoring, stored in NVS
 * 
 * @return true 
 * @return false 
 */
bool evse_is_rcm(void);

/**
 * @brief Set consumption limit
 * 
 * @param consumption_limit consumption in Ws
 */
void evse_set_consumption_limit(uint32_t consumption_limit);

/**
 * @brief Get elapsed limit
 *
 * @return elapsed in s
 */
uint32_t evse_get_elapsed_limit(void);

/**
 * @brief Set elapsed limit
 * 
 * @param elapsed_limit elapsed in s
 */
void evse_set_elapsed_limit(uint32_t elapsed_limit);

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
 * @brief Get elapsed limit, stored in NVS
 *
 * @return elapsed in s
 */
uint32_t evse_get_default_elapsed_limit(void);

/**
 * @brief Set elapsed limit, stored in NVS
 * 
 * @param elapsed_limit elapsed in s
 */
void evse_set_default_elapsed_limit(uint32_t elapsed_limit);

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
