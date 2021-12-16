#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "pilot.h"
#include "board_config.h"

#define PILOT_PWM_TIMER LEDC_TIMER_0
#define PILOT_PWM_CHANNEL LEDC_CHANNEL_0
#define PILOT_PWM_SPEED_MODE LEDC_HIGH_SPEED_MODE
#define PILOT_PWM_MAX_DUTY 1023
#define PILOT_SENS_SAMPLES 64

static const char* TAG = "peripherals";

static esp_adc_cal_characteristics_t pilot_sens_adc_char;

void pilot_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT, //1023
        .freq_hz = 1000,
        .speed_mode = PILOT_PWM_SPEED_MODE,
        .timer_num = PILOT_PWM_TIMER
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = board_config.pilot_pwm_gpio,
        .channel = PILOT_PWM_CHANNEL,
        .speed_mode = PILOT_PWM_SPEED_MODE,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PILOT_PWM_TIMER,
        .duty = PILOT_PWM_MAX_DUTY
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_fade_func_install(0);

    ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.pilot_sens_adc_channel, ADC_ATTEN_DB_11));

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &pilot_sens_adc_char);
}

void pilot_pwm_set_level(bool level)
{
    ESP_LOGD(TAG, "Set level %d", level);

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

    ESP_LOGD(TAG, "Set amp %d duty %d", amps, duty);

    ledc_set_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, duty);
    ledc_update_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL);
}

pilot_voltage_t pilot_get_voltage(void)
{
    uint32_t high = 0;
    uint32_t low = 3300;

    for (int i = 0; i < 100; i++) {
        int adc_reading = adc1_get_raw(board_config.pilot_sens_adc_channel);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &pilot_sens_adc_char);
        if (voltage > high) {
            high = voltage;
        } else if (voltage < low) {
            low = voltage;
        }
        ets_delay_us(100);
    }

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

    ESP_LOGD(TAG, "CP read: %dmV - %dmV (%d)", low, high, voltage);

    return voltage;
}
