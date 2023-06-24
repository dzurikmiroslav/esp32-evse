#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "rcm.h"
#include "board_config.h"
#include "evse.h"


static void IRAM_ATTR rcm_isr_handler(void* arg)
{
    evse_rcm_triggered_isr();
}

void rcm_init(void)
{
    if (board_config.rcm) {
        gpio_config_t io_conf = {};

        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_test_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_INTR_POSEDGE;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        ESP_ERROR_CHECK(gpio_isr_handler_add(board_config.rcm_gpio, rcm_isr_handler, NULL));
    }
}

void rcm_test(void)
{
    gpio_set_level(board_config.rcm_test_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(board_config.rcm_test_gpio, 0);
}