#ifndef PILOT_H_
#define PILOT_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Up pilot volate
 * 
 */
typedef enum
{
    PILOT_VOLTAGE_12,
    PILOT_VOLTAGE_9,
    PILOT_VOLTAGE_6,
    PILOT_VOLTAGE_3,
    PILOT_VOLTAGE_1 // below 3V
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
 */
void pilot_measure(void);

/**
 * @brief After pilot_measure, return up measured value
 * 
 * @return pilot_voltage_t 
 */
pilot_voltage_t pilot_get_up_voltage(void);

/**
 * @brief After pilot_measure, return down voltage is -12V tollerant
 * 
 * @return true when down volage is -12V tolerant
 * @return false when down volage are greater than -12V tolerant
 */
bool pilot_is_down_voltage_n12(void);

#endif /* PILOT_H_ */
