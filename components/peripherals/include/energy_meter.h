#ifndef ENERGY_METER_H_
#define ENERGY_METER_H_

#include <stdbool.h>
#include <stdint.h>

void energy_meter_init(void);

void energy_meter_process(void);

/*
 * Get charging actual power (1W)
 */
uint16_t energy_meter_get_power(void);

/**
 * Get charging elapsed time (unit 1s)
 */
uint32_t energy_meter_get_session_elapsed(void);

/**
 * Get charging consumption (unit 1Ws)
 */
uint32_t energy_meter_get_session_consumption(void);

void energy_meter_get_voltage(float *voltage);

void energy_meter_get_current(float *current);


#endif /* ENERGY_METER_H_ */
