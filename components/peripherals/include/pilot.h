#ifndef PILOT_H_
#define PILOT_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PILOT_VOLTAGE_12,
    PILOT_VOLTAGE_9,
    PILOT_VOLTAGE_6,
    PILOT_VOLTAGE_3,
    PILOT_VOLTAGE_1 // below 3V
} pilot_voltage_t;

void pilot_init(void);

void pilot_pwm_set_level(bool level);

void pilot_pwm_set_amps(uint8_t amps);

bool pilot_is_pwm(void);

void pilot_measure(void);

pilot_voltage_t pilot_get_voltage(void);

/**
 * Test negative voltage to 12V when PWM run
 */
bool pilot_test_diode(void);

#endif /* PILOT_H_ */
