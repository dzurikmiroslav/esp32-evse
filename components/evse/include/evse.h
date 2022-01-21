#ifndef EVSE_H_
#define EVSE_H_

#include <stdbool.h>
#include <stdint.h>

#define evse_state_is_session(state)        (state >= EVSE_STATE_B && state <= EVSE_STATE_D)
#define evse_state_is_charging(state)       (state >= EVSE_STATE_C && state <= EVSE_STATE_D)

typedef enum
{
    EVSE_STATE_A,
    EVSE_STATE_B,
    EVSE_STATE_C,
    EVSE_STATE_D,
    EVSE_STATE_ERROR,  
    EVSE_STATE_DISABLED   
} evse_state_t;

typedef enum
{
    EVSE_ERR_NONE,
    EVSE_ERR_PILOT_FAULT,
    EVSE_ERR_DIODE_SHORT
} evse_err_t;

void evse_init(void);

/**
 * Enable evse controller
 */
void evse_enable(void);

/**
 * Disable evse controller, for system update, restart, etc..
 */
void evse_disable(void);

void evse_process(void);

evse_state_t evse_get_state(void);

uint16_t evse_get_error(void);

/**
 * Get charging current
 */
float evse_get_chaging_current(void);

/**
 * Set charging current
 */
void evse_set_chaging_current(float charging_current);

/**
 * Get default charging current, stored in NVS
 */
float evse_get_default_chaging_current(void);

/**
 * Set default charging current, stored in NVS
 */
void evse_set_default_chaging_current(float charging_current);

/**
 * Is required authorization to start charging
 */
bool evse_is_require_auth(void);

/**
 * Set authorization required to start charging
 */
void evse_set_require_auth(bool require_auth);

/**
 * Authorize to start charging when authorization is required
 */
void evse_authorize(void);

/**
 * Pending authorize to start charging
 */
bool evse_is_pending_auth(void);

bool evse_is_paused(void);

void evse_pause(void);

void evse_unpause(void);

#endif /* EVSE_H_ */
