#include "rcm.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board_config.h"
#include "evse.h"

// static bool do_test = false;

// static bool triggered = false;

// static bool test_triggered = false;

// static void IRAM_ATTR rcm_isr_handler(void* arg)
// {
//     if (!do_test) {
//         triggered = true;
//     } else {
//         test_triggered = true;
//     }
// }

void rcm_init(void)
{
    if (board_config.rcm) {
        gpio_config_t io_conf = { 0 };

        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_test_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        // io_conf.intr_type = GPIO_INTR_POSEDGE;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        // ESP_ERROR_CHECK(gpio_isr_handler_add(board_config.rcm_gpio, rcm_isr_handler, NULL));
    }
}

bool rcm_test(void)
{
    // do_test = true;
    // test_triggered = false;

    // gpio_set_level(board_config.rcm_test_gpio, 1);
    // vTaskDelay(pdMS_TO_TICKS(100));
    // gpio_set_level(board_config.rcm_test_gpio, 0);

    // do_test = false;

    // return test_triggered;

    gpio_set_level(board_config.rcm_test_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    bool success = gpio_get_level(board_config.rcm_gpio) == 1;
    gpio_set_level(board_config.rcm_test_gpio, 0);

    return success;
}

bool rcm_is_triggered(void)
{
    // bool _triggered = triggered;
    // if (gpio_get_level(board_config.rcm_gpio) == 0) {
    //     triggered = false;
    // }
    // return _triggered;
    if (gpio_get_level(board_config.rcm_gpio) == 1) {
        vTaskDelay(pdMS_TO_TICKS(1));
        return gpio_get_level(board_config.rcm_gpio) == 1;
    }

    return false;
}