#ifndef LED_H_
#define LED_H_

#include <stdint.h>

typedef enum
{
    LED_ID_WIFI,
    LED_ID_CHARGING,
    LED_ID_ERROR,
    LED_ID_MAX
} led_id_t;

void led_init(void);

void led_set_state(led_id_t led_id, uint16_t ontime, uint16_t offtime);

#define led_set_on(led_id)      led_set_state(led_id, 1, 0)
#define led_set_off(led_id)     led_set_state(led_id, 0, 0)

#endif /* LED_H_ */
