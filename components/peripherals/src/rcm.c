#include "rcm.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "board_config.h"

static SemaphoreHandle_t triggered_sem = NULL;

static void IRAM_ATTR rcm_isr_handler(void* arg)
{
    BaseType_t higher_task_woken = pdFALSE;

    xSemaphoreGiveFromISR(triggered_sem, &higher_task_woken);

    if (higher_task_woken) {
        portYIELD_FROM_ISR();
    }
}

void rcm_init(void)
{
    if (board_cfg_is_rcm(board_config)) {
        triggered_sem = xSemaphoreCreateBinary();

        gpio_config_t io_conf = { 0 };

        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_test_gpio);
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = BIT64(board_config.rcm_gpio);
        io_conf.intr_type = GPIO_INTR_POSEDGE;
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        ESP_ERROR_CHECK(gpio_isr_handler_add(board_config.rcm_gpio, rcm_isr_handler, NULL));
    }
}

bool rcm_test(void)
{   
    xSemaphoreTake(triggered_sem, 0);

    gpio_set_level(board_config.rcm_test_gpio, 1);
    bool success = xSemaphoreTake(triggered_sem, pdMS_TO_TICKS(board_config.rcm_test_delay));
    gpio_set_level(board_config.rcm_test_gpio, 0);

    return success;
}

bool rcm_is_triggered(void)
{
    if (xSemaphoreTake(triggered_sem, 0)) {
        vTaskDelay(pdMS_TO_TICKS(1));  // prevent from noise triggering
        return gpio_get_level(board_config.rcm_gpio);
    }
    return false;
}