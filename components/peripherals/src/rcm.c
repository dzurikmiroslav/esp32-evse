#include "esp_log.h"
#include "driver/gpio.h"

#include "rcm.h"
#include "board_config.h"

static const char* TAG = "rcm";

SemaphoreHandle_t rcm_semhr;

static bool do_test = false;

static bool triggered = false;

static bool test_triggered = false;

static void IRAM_ATTR rcm_isr_handler(void* arg)
{
    BaseType_t higher_task_woken = pdFALSE;

    if (!do_test) {
        triggered = true;
    } else {
        test_triggered = true;
    }

    if (higher_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void rcm_init(void)
{
    if (board_config.rcm) {
        rcm_semhr = xSemaphoreCreateBinary();

        gpio_config_t io_conf = {};

        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << board_config.rcm_test_gpio;
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.intr_type = GPIO_INTR_POSEDGE;
        io_conf.pin_bit_mask = 1ULL << board_config.rcm_gpio;
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        ESP_ERROR_CHECK(gpio_isr_handler_add(board_config.rcm_gpio, rcm_isr_handler, NULL));
    }
}

bool rcm_test(void)
{
    do_test = true;
    test_triggered = false;
    gpio_set_level(board_config.rcm_test_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(board_config.rcm_test_gpio, 0);

    do_test = false;

    return test_triggered;
}

bool rcm_was_triggered(void)
{
    bool _triggered = triggered;
    triggered = false;
    return _triggered;
}