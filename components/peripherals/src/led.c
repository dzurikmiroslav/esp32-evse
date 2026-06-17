#include "led.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "board_config.h"

#define BLOCK_TIME 10

typedef struct {
    gpio_num_t gpio;
    bool on : 1;
    uint16_t ontime;
    uint16_t offtime;
    TimerHandle_t timer;
} led_t;

static const char* TAG = "led";

static led_t s_leds[LED_ID_MAX];

void led_init(void)
{
    for (int i = 0; i < LED_ID_MAX; i++) {
        s_leds[i].timer = NULL;
        s_leds[i].gpio = GPIO_NUM_NC;
    }

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = 0,
    };

    if (board_config.leds.wifi_gpio != -1) {
        s_leds[LED_ID_WIFI].gpio = board_config.leds.wifi_gpio;
        io_conf.pin_bit_mask |= BIT64(board_config.leds.wifi_gpio);
    }

    if (board_config.leds.charging_gpio != -1) {
        s_leds[LED_ID_CHARGING].gpio = board_config.leds.charging_gpio;
        io_conf.pin_bit_mask |= BIT64(board_config.leds.charging_gpio);
    }

    if (board_config.leds.error_gpio != -1) {
        s_leds[LED_ID_ERROR].gpio = board_config.leds.error_gpio;
        io_conf.pin_bit_mask |= BIT64(board_config.leds.error_gpio);
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }
}

static void timer_callback(TimerHandle_t xTimer)
{
    led_t* led = (led_t*)pvTimerGetTimerID(xTimer);

    led->on = !led->on;
    gpio_set_level(led->gpio, led->on);

    uint16_t next = led->on ? led->ontime : led->offtime;
    if (next > 0) {
        xTimerChangePeriod(xTimer, pdMS_TO_TICKS(next), BLOCK_TIME);
    }
}

void led_set_state(led_id_t led_id, uint16_t ontime, uint16_t offtime)
{
    led_t* led = &s_leds[led_id];
    if (led->gpio == GPIO_NUM_NC) {
        return;
    }

    if (led->timer != NULL) {
        xTimerStop(led->timer, BLOCK_TIME);
    }

    led->ontime = ontime;
    led->offtime = offtime;

    if (ontime == 0) {
        ESP_LOGD(TAG, "Set led %d off", led_id);
        led->on = false;
        gpio_set_level(led->gpio, led->on);
    } else if (offtime == 0) {
        ESP_LOGD(TAG, "Set led %d on", led_id);
        led->on = true;
        gpio_set_level(led->gpio, led->on);
    } else {
        ESP_LOGD(TAG, "Set led %d blink (on: %d off: %d)", led_id, ontime, offtime);

        led->on = true;
        gpio_set_level(led->gpio, led->on);

        if (led->timer == NULL) {
            led->timer = xTimerCreate("led_timer", pdMS_TO_TICKS(ontime), pdFALSE, (void*)led, timer_callback);
        } else {
            xTimerChangePeriod(led->timer, pdMS_TO_TICKS(ontime), BLOCK_TIME);
        }
        xTimerStart(led->timer, BLOCK_TIME);
    }
}
