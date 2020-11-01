#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "peripherals.h"

#include "board_config.h"

#define PILOT_PWM_TIMER         LEDC_TIMER_0
#define PILOT_PWM_CHANNEL       LEDC_CHANNEL_0
#define PILOT_PWM_SPEED_MODE    LEDC_HIGH_SPEED_MODE
#define PILOT_PWM_MAX_DUTY      1023

#define PILOT_SENS_CHANNEL      ADC1_CHANNEL_7

#define LED_TIMER               LEDC_TIMER_1
#define LED_SPEED_MODE          LEDC_LOW_SPEED_MODE

static const char *TAG = "peripherals";

static esp_adc_cal_characteristics_t pilot_sens_adc_char;

static ledc_channel_t led_channel[] = { LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3 };

void pilot_init(void)
{
// @formatter:off
    ledc_timer_config_t ledc_timer = {
            .duty_resolution = LEDC_TIMER_10_BIT,   //1023
            .freq_hz    = 1000,
            .speed_mode = PILOT_PWM_SPEED_MODE,
            .timer_num  = PILOT_PWM_TIMER
    };
// @formatter:on
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

// @formatter:off
    ledc_channel_config_t ledc_channel = {
            .gpio_num   = board_config.pilot_pwm_gpio,
            .channel    = PILOT_PWM_CHANNEL,
            .speed_mode = PILOT_PWM_SPEED_MODE,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = PILOT_PWM_TIMER,
            .duty       = PILOT_PWM_MAX_DUTY
    };
// @formatter:on
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    //ledc_fade_func_install(0);

    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_12));
    ESP_ERROR_CHECK(adc1_config_channel_atten((adc1_channel_t ) board_config.pilot_sens_adc_channel, ADC_ATTEN_DB_11));

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &pilot_sens_adc_char);

}

void pilot_pwm_set_level(bool level)
{
    ledc_set_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, level ? PILOT_PWM_MAX_DUTY : 0);
    ledc_update_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL);
}

void pilot_pwm_set_amps(uint8_t amps)
{
    uint32_t duty = 0;
    if ((amps >= 6) && (amps <= 51)) {
        // amps = (duty cycle %) X 0.6
        duty = amps * (PILOT_PWM_MAX_DUTY / 60);
    } else if ((amps > 51) && (amps <= 80)) {
        // amps = (duty cycle % - 64) X 2.5
        duty = (amps * (PILOT_PWM_MAX_DUTY / 250)) + (64 * (PILOT_PWM_MAX_DUTY / 100));
    } else {
        ESP_LOGE(TAG, "Try set invalid ampere value");
        return;
    }

    ESP_LOGI(TAG, "Set amp %d duty %d", amps, duty);

    ledc_set_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, duty);
    ledc_update_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL);
}

pilot_voltage_t pilot_get_voltage(void)
{
    uint32_t high = 0;
    uint32_t low = 3300;

    for (int i = 0; i < 100; i++) {
        int adc_reading = adc1_get_raw(ADC1_CHANNEL_7);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &pilot_sens_adc_char);
        if (voltage > high) {
            high = voltage;
        } else if (voltage < low) {
            low = voltage;
        }
        ets_delay_us(100);
    }

    ESP_LOGI(TAG, "Pilot read: %dmV - %dmV", low, high);

    pilot_voltage_t voltage;

    if (high >= board_config.pilot_sens_down_treshold_12) {
        voltage = PILOT_VOLTAGE_12;
    } else if (high >= board_config.pilot_sens_down_treshold_9) {
        voltage = PILOT_VOLTAGE_9;
    } else if (high >= board_config.pilot_sens_down_treshold_6) {
        voltage = PILOT_VOLTAGE_6;
    } else if (high >= board_config.pilot_sens_down_treshold_3) {
        voltage = PILOT_VOLTAGE_3;
    } else {
        voltage = PILOT_VOLTAGE_1;
    }

    return voltage;
}

void ac_relay_init(void)
{
    gpio_pad_select_gpio(board_config.ac_relay_gpio);
    gpio_set_direction(board_config.ac_relay_gpio, GPIO_MODE_OUTPUT);
}

void ac_relay_set_state(bool state)
{
    gpio_set_level(board_config.ac_relay_gpio, state);
}

void led_init(void)
{
// @formatter:off
    ledc_timer_config_t ledc_timer = {
            .duty_resolution = LEDC_TIMER_10_BIT,   //1023
            .freq_hz    = 1,
            .speed_mode = LED_SPEED_MODE,
            .timer_num  = LED_TIMER
    };
// @formatter:on
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

// @formatter:off
    ledc_channel_config_t ledc_channel = {
            .speed_mode = LED_SPEED_MODE,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = LED_TIMER,
            .duty       = 0
    };
// @formatter:on

    if (board_config.led_wifi) {
        ledc_channel.gpio_num = board_config.led_wifi_gpio;
        ledc_channel.channel = led_channel[LED_ID_WIFI];
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    if (board_config.led_charging) {
        ledc_channel.gpio_num = board_config.led_charging_gpio;
        ledc_channel.channel = led_channel[LED_ID_CHARGING];
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    if (board_config.led_error) {
        ledc_channel.gpio_num = board_config.led_error_gpio;
        ledc_channel.channel = led_channel[LED_ID_ERROR];
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    //ledc_fade_func_install(0);
}

void led_set_state(led_id_t led_id, led_state_t state)
{
    switch (state) {
        case LED_STATE_ON:
            ledc_set_duty(LED_SPEED_MODE, led_channel[led_id], 1023);
            break;
        case LED_STATE_OFF:
            ledc_set_duty(LED_SPEED_MODE, led_channel[led_id], 0);
            break;
        case LED_STATE_DUTY_5:
            ledc_set_duty(LED_SPEED_MODE, led_channel[led_id], 1023 / 100 * 5);
            break;
        case LED_STATE_DUTY_50:
            ledc_set_duty(LED_SPEED_MODE, led_channel[led_id], 1023 / 100 * 50);
            break;
        case LED_STATE_DUTY_75:
            ledc_set_duty(LED_SPEED_MODE, led_channel[led_id], 1023 / 100 * 95);
            break;
    }
    ledc_update_duty(LED_SPEED_MODE, led_channel[led_id]);
}

