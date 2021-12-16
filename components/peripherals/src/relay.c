#include "esp_log.h"
#include "driver/gpio.h"

#include "relay.h"
#include "board_config.h"

static const char* TAG = "relay";

void ac_relay_init(void)
{
    gpio_pad_select_gpio(board_config.ac_relay_gpio);
    gpio_set_direction(board_config.ac_relay_gpio, GPIO_MODE_OUTPUT);
}

void ac_relay_set_state(bool state)
{
    gpio_set_level(board_config.ac_relay_gpio, state);
}
