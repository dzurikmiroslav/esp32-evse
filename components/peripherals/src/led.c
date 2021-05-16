#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#include "led.h"
#include "board_config.h"

#define BLOCK_TIME              10

static const char *TAG = "led";

static struct led_s
{
    gpio_num_t gpio;
    bool on :1;
    uint16_t ontime, offtime;
    TimerHandle_t timer;
} leds[3];

void led_init(void)
{
    for (int i = 0; i < LED_ID_MAX; i++) {
        leds[i].timer = NULL;
        leds[i].gpio = GPIO_NUM_NC;
    }

    gpio_config_t io_conf;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 0;

    if (board_config.led_wifi) {
        leds[LED_ID_WIFI].gpio = board_config.led_wifi_gpio;
        io_conf.pin_bit_mask |= 1ULL << board_config.led_wifi_gpio;
    }

    if (board_config.led_charging) {
        leds[LED_ID_CHARGING].gpio = board_config.led_charging_gpio;
        io_conf.pin_bit_mask |= 1ULL << board_config.led_charging_gpio;
    }

    if (board_config.led_error) {
        leds[LED_ID_ERROR].gpio = board_config.led_error_gpio;
        io_conf.pin_bit_mask |= 1ULL << board_config.led_error_gpio;
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }
}

static void timer_callback(TimerHandle_t xTimer)
{
    struct led_s *led = (struct led_s*) pvTimerGetTimerID(xTimer);

    led->on = !led->on;
    gpio_set_level(led->gpio, led->on);

    xTimerChangePeriod(xTimer, (led->on ? led->ontime : led->offtime) / portTICK_RATE_MS, BLOCK_TIME);
}

void led_set_state(led_id_t led_id, uint16_t ontime, uint16_t offtime)
{
    struct led_s *led = &leds[led_id];
    if (led->gpio != GPIO_NUM_NC) {
        if (led->timer != NULL) {
            xTimerStop(led->timer, BLOCK_TIME);
            led->timer = NULL;
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
                led->timer = xTimerCreate("led_timer", ontime / portTICK_RATE_MS, pdFALSE, (void*) led, timer_callback);
            }
            xTimerStart(led->timer, BLOCK_TIME);
        }
    }
}

