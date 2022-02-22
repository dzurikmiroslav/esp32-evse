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
    EVSE_STATE_ERROR,
    EVSE_STATE_DISABLED
} evse_state_t;

/**
 * @brief Error codes when state is EVSE_STATE_ERROR
 *
 */
typedef enum
{
    EVSE_ERR_NONE,
    EVSE_ERR_PILOT_FAULT,
    EVSE_ERR_DIODE_SHORT
} evse_err_t;

/**
 * @brief Initialize evse
 *
 */
void evse_init(void);

/**
 * @brief Enable evse controller
 *
 */
void evse_enable(void);

/**
 * @brief Disable evse controller, for system update, restart, etc..
 *
 */
void evse_disable(void);

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
 * @brief Return detailed error code when state is EVSE_STATE_ERROR
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
 * @brief Return evse is paused
 *
 * @return true
 * @return false
 */
bool evse_is_paused(void);

/**
 * @brief Pause evse
 *
 */
void evse_pause(void);

/**
 * @brief Unpause evse
 *
 */
void evse_unpause(void);

#endif /* EVSE_H_ */
