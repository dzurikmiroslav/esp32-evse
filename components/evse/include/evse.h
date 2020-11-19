#ifndef EVSE_H_
#define EVSE_H_

typedef enum
{
    EVSE_STATE_A, EVSE_STATE_B, EVSE_STATE_C, EVSE_STATE_D, EVSE_STATE_E, EVSE_STATE_F
} evse_state_t;


void evse_init(void);

void evse_process(void);

void evse_mock(evse_state_t state);

/**
 * Get charging elapsed time (unit 1s)
 */
uint32_t evse_get_session_elapsed(void);

/**
 * Get charging consumption (unit 1Wh)
 */
uint32_t evse_get_session_consumption(void);

/**
 * Get charging actual power (1W)
 */
uint16_t evse_get_session_actual_power(void);

evse_state_t evse_get_state(void);

uint16_t evse_get_error(void);

/**
 * Get charging current (unit 0.1A)
 */
uint16_t evse_get_chaging_current(void);

/**
 * Set charging current (unit 0.1A)
 */
void evse_set_chaging_current(uint16_t charging_current);


#endif /* EVSE_H_ */
