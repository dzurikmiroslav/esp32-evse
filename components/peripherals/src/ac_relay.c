
#include "ac_relay.h"

#include <driver/gpio.h>
#include <esp_log.h>

#include "board_config.h"

static const char* TAG = "ac_relay";

void ac_relay_init(void)
{
    gpio_config_t conf = {
        .pin_bit_mask = BIT64(board_config.ac_relay.gpios[BOARD_CFG_AC_RELAY_GPIO_L1]),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (board_cfg_is_ac_relay_l2_l3(board_config)) {
        conf.pin_bit_mask |= BIT64(board_config.ac_relay.gpios[BOARD_CFG_AC_RELAY_GPIO_L2_L3]);
    }
    ESP_ERROR_CHECK(gpio_config(&conf));
}

void ac_relay_set_state(bool state)
{
    ESP_LOGI(TAG, "Set relay: %d", state);
    gpio_set_level(board_config.ac_relay.gpios[BOARD_CFG_AC_RELAY_GPIO_L1], state);
    if (board_cfg_is_ac_relay_l2_l3(board_config)) {
        gpio_set_level(board_config.ac_relay.gpios[BOARD_CFG_AC_RELAY_GPIO_L2_L3], state);
    }
}
