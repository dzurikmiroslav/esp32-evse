#ifndef PERIPHERALS_H_
#define PERIPHERALS_H_

/**
 * Pilot control
 */
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

pilot_voltage_t pilot_get_voltage(void);

/**
 * Relay
 */

void ac_relay_init(void);

void ac_relay_set_state(bool state);

/**
 * LED control
 */

typedef enum
{
    LED_ID_WIFI, LED_ID_CHARGING, LED_ID_ERROR
} led_id_t;

typedef enum
{
    LED_STATE_ON, LED_STATE_OFF, LED_STATE_DUTY_5, LED_STATE_DUTY_50, LED_STATE_DUTY_75
} led_state_t;

void led_init();

void led_set_state(led_id_t led_id, led_state_t state);

#endif /* PERIPHERALS_H_ */
