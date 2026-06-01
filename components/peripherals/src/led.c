#include "led.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "board_config.h"

#define BLOCK_TIME 10

static const char* TAG = "led";

static struct led_s {
    gpio_num_t gpio;
    bool on : 1;
    uint16_t ontime, offtime;
    TimerHandle_t timer;
} leds[3];

void led_init(void)
{
    for (int i = 0; i < LED_ID_MAX; i++) {
        leds[i].timer = NULL;
        leds[i].gpio = GPIO_NUM_NC;
    }

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = 0,
    };

    if (board_config.leds.wifi_gpio != -1) {
        leds[LED_ID_WIFI].gpio = board_config.leds.wifi_gpio;
        io_conf.pin_bit_mask |= BIT64(board_config.leds.wifi_gpio);
    }

    if (board_config.leds.charging_gpio != -1) {
        leds[LED_ID_CHARGING].gpio = board_config.leds.charging_gpio;
        io_conf.pin_bit_mask |= BIT64(board_config.leds.charging_gpio);
    }

    if (board_config.leds.error_gpio != -1) {
        leds[LED_ID_ERROR].gpio = board_config.leds.error_gpio;
        io_conf.pin_bit_mask |= BIT64(board_config.leds.error_gpio);
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }
}

static void timer_callback(TimerHandle_t xTimer)
{
    struct led_s* led = (struct led_s*)pvTimerGetTimerID(xTimer);

    led->on = !led->on;
    gpio_set_level(led->gpio, led->on);

    // Guard against a zero period: led_set_state() may have switched this led to
    // a solid state (ontime/offtime == 0) while this callback was already in
    // flight. xTimerChangePeriod() with 0 ticks trips configASSERT() in the
    // FreeRTOS timer task and panics, so don't re-arm in that case.
    uint16_t next = led->on ? led->ontime : led->offtime;
    if (next > 0) {
        xTimerChangePeriod(xTimer, pdMS_TO_TICKS(next), BLOCK_TIME);
    }
}

void led_set_state(led_id_t led_id, uint16_t ontime, uint16_t offtime)
{
    struct led_s* led = &leds[led_id];
    if (led->gpio == GPIO_NUM_NC) {
        return;
    }

    // Keep one persistent timer per led and reuse it. Previously the timer was
    // stopped and its handle dropped (leaking a timer on every reconfigure) and
    // recreated each time, which under rapid reconfiguration left stopped-but-
    // pending callbacks firing with stale on/off times.
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
