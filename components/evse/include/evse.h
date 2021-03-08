#ifndef EVSE_H_
#define EVSE_H_

#include <stdbool.h>
#include <stdint.h>

#define evse_state_in_session(state)             (state >= EVSE_STATE_B && state <= EVSE_STATE_D)
#define evse_state_relay_closed(state)           (state >= EVSE_STATE_C && state <= EVSE_STATE_D)

typedef enum
{
    EVSE_STATE_A, EVSE_STATE_B, EVSE_STATE_C, EVSE_STATE_D, EVSE_STATE_E, EVSE_STATE_F
} evse_state_t;

typedef enum
{
    EVSE_ERR_NONE,
    EVSE_ERR_PILOT_FAULT
} evse_err_t;

void evse_init(void);

void evse_enable(void);

void evse_disable(void);

bool evse_try_disable(void);

bool evse_process(void);

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


#endif /* EVSE_H_ */
