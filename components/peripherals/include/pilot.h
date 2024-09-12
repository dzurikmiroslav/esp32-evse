#ifndef PILOT_H_
#define PILOT_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Up pilot volate
 *
 */
typedef enum {
    PILOT_VOLTAGE_12,
    PILOT_VOLTAGE_9,
    PILOT_VOLTAGE_6,
    PILOT_VOLTAGE_3,
    PILOT_VOLTAGE_1  // below 3V
} pilot_voltage_t;

/**
 * @brief Initialize pilot
 *
 */
void pilot_init(void);

/**
 * @brief Set pilot to + or - 12V output
 *
 * @param level When true output is +12V false means -12V
 */
void pilot_set_level(bool level);

/**
 * @brief Set pilot +-12V pwm output
 *
 * @param amps current in A*10
 */
void pilot_set_amps(uint16_t amps);

/**
 * @brief Measure pilot up and down voltage
 *
 * @param up_voltage
 * @param down_voltage_n12 true when down volage is -12V tolerant otherwise false
 */
void pilot_measure(pilot_voltage_t *up_voltage, bool *down_voltage_n12);

#endif /* PILOT_H_ */
