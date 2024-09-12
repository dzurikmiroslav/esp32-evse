#include "pilot.h"

#include <driver/ledc.h>
#include <esp_log.h>
#include <rom/ets_sys.h>

#include "adc.h"
#include "board_config.h"

#define PILOT_PWM_TIMER      LEDC_TIMER_0
#define PILOT_PWM_CHANNEL    LEDC_CHANNEL_0
#define PILOT_PWM_SPEED_MODE LEDC_LOW_SPEED_MODE
#define PILOT_PWM_DUTY_RES   LEDC_TIMER_10_BIT
#define PILOT_PWM_MAX_DUTY   1023

#define PILOT_SAMPLES 64

static const char* TAG = "pilot";

void pilot_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = PILOT_PWM_SPEED_MODE,
        .timer_num = PILOT_PWM_TIMER,
        .duty_resolution = PILOT_PWM_DUTY_RES,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = PILOT_PWM_SPEED_MODE,
        .channel = PILOT_PWM_CHANNEL,
        .timer_sel = PILOT_PWM_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = board_config.pilot_pwm_gpio,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_stop(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, 1));

    ledc_fade_func_install(0);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.pilot_adc_channel, &config));
}

void pilot_set_level(bool level)
{
    ESP_LOGI(TAG, "Set level %d", level);

    ledc_stop(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, level);
}

void pilot_set_amps(uint16_t amps)
{
    uint32_t duty = 0;

    if ((amps >= 60) && (amps <= 510)) {
        // amps = (duty cycle %) X 0.6
        duty = (amps / 10) * (PILOT_PWM_MAX_DUTY / 60);
    } else if ((amps > 510) && (amps <= 800)) {
        // amps = (duty cycle % - 64) X 2.5
        duty = ((amps / 10) * (PILOT_PWM_MAX_DUTY / 250)) + (64 * (PILOT_PWM_MAX_DUTY / 100));
    } else {
        ESP_LOGE(TAG, "Try set invalid ampere value %d A*10", amps);
        return;
    }

    ESP_LOGI(TAG, "Set amp %dA*10 duty %lu/%d", amps, duty, PILOT_PWM_MAX_DUTY);

    ledc_set_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, duty);
    ledc_update_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL);
}

void pilot_measure(pilot_voltage_t* up_voltage, bool* down_voltage_n12)
{
    int high = 0;
    int low = 3300;

    for (int i = 0; i < 100; i++) {
        int adc_reading;
        adc_oneshot_read(adc_handle, board_config.pilot_adc_channel, &adc_reading);

        if (adc_reading > high) {
            high = adc_reading;
        } else if (adc_reading < low) {
            low = adc_reading;
        }
        ets_delay_us(100);
    }

    adc_cali_raw_to_voltage(adc_cali_handle, high, &high);
    adc_cali_raw_to_voltage(adc_cali_handle, low, &low);

    ESP_LOGV(TAG, "Measure: %dmV - %dmV", low, high);

    if (high >= board_config.pilot_down_threshold_12) {
        *up_voltage = PILOT_VOLTAGE_12;
    } else if (high >= board_config.pilot_down_threshold_9) {
        *up_voltage = PILOT_VOLTAGE_9;
    } else if (high >= board_config.pilot_down_threshold_6) {
        *up_voltage = PILOT_VOLTAGE_6;
    } else if (high >= board_config.pilot_down_threshold_3) {
        *up_voltage = PILOT_VOLTAGE_3;
    } else {
        *up_voltage = PILOT_VOLTAGE_1;
    }

    *down_voltage_n12 = low <= board_config.pilot_down_threshold_n12;

    ESP_LOGV(TAG, "Up voltage %d", *up_voltage);
    ESP_LOGV(TAG, "Down voltage below 12V %d", *down_voltage_n12);
}
