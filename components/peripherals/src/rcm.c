#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "rcm.h"
#include "board_config.h"
#include "evse.h"

static bool do_test = false;

static bool triggered = false;

static bool test_triggered = false;

static void rcm_timer_callback(void* arg)
{
    if (gpio_get_level(board_config.rcm_gpio) == 1) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (gpio_get_level(board_config.rcm_gpio) == 1) {
            if (do_test) {
                test_triggered = true;
            } else {
                triggered = true;
            }
        }
    }
}

void rcm_init(void)
{
    if (board_config.rcm) {
        gpio_config_t io_conf = {};

        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_test_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &rcm_timer_callback,
            .name = "rcm"
        };

        vTaskDelay(pdMS_TO_TICKS(100));  //wait for gpio setup
        esp_timer_handle_t periodic_timer;
        ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10000));
    }
}

bool rcm_test(void)
{
    do_test = true;
    test_triggered = false;

    gpio_set_level(board_config.rcm_test_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(board_config.rcm_test_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(50));  //wait for write gpio

    do_test = false;

    return test_triggered;
}

bool rcm_was_triggered(void)
{
    bool _triggered = triggered;
    triggered = false;
    return _triggered;
}