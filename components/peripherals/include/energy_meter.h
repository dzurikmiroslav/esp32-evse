#ifndef ENERGY_METER_H_
#define ENERGY_METER_H_

#include <stdbool.h>
#include <stdint.h>


void energy_meter_init(void);

float energy_meter_actual_power(void);

#endif /* ENERGY_METER_H_ */
