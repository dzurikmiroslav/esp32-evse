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

#define PILOT_SAMPLES       250  // during 1000us pilot period, should be divisible
#define PILOT_HI_LO_SAMPLES 3

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
        .gpio_num = board_config.pilot.gpio,
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
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.pilot.adc_channel, &config));
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
    int high_samples[PILOT_HI_LO_SAMPLES];
    int low_samples[PILOT_HI_LO_SAMPLES];

    for (int i = 0; i < PILOT_HI_LO_SAMPLES; i++) {
        high_samples[i] = 0;
        low_samples[i] = 3300;
    }

    for (int i = 0; i < PILOT_SAMPLES; i++) {
        int adc_reading;
        adc_oneshot_read(adc_handle, board_config.pilot.adc_channel, &adc_reading);

        for (int j = 0; j < PILOT_HI_LO_SAMPLES; j++) {
            if (adc_reading > high_samples[j]) {
                for (int m = PILOT_HI_LO_SAMPLES - 1; m > j; m--) high_samples[m] = high_samples[m - 1];
                high_samples[j] = adc_reading;
                break;
            }
        }

        for (int j = 0; j < PILOT_HI_LO_SAMPLES; j++) {
            if (adc_reading < low_samples[j]) {
                for (int m = PILOT_HI_LO_SAMPLES - 1; m > j; m--) low_samples[m] = low_samples[m - 1];
                low_samples[j] = adc_reading;
                break;
            }
        }

        ets_delay_us(1000 / PILOT_SAMPLES);  // 1000us pilot period
    }

    int high = 0;
    int low = 0;

    for (int i = 0; i < PILOT_HI_LO_SAMPLES; i++) {
        high += high_samples[i];
        low += low_samples[i];
    }

    high /= PILOT_HI_LO_SAMPLES;
    low /= PILOT_HI_LO_SAMPLES;

    adc_cali_raw_to_voltage(adc_cali_handle, high, &high);
    adc_cali_raw_to_voltage(adc_cali_handle, low, &low);

    ESP_LOGV(TAG, "Measure: %dmV - %dmV", low, high);

    if (high >= board_config.pilot.levels[BOARD_CFG_PILOT_LEVEL_12]) {
        *up_voltage = PILOT_VOLTAGE_12;
    } else if (high >= board_config.pilot.levels[BOARD_CFG_PILOT_LEVEL_9]) {
        *up_voltage = PILOT_VOLTAGE_9;
    } else if (high >= board_config.pilot.levels[BOARD_CFG_PILOT_LEVEL_6]) {
        *up_voltage = PILOT_VOLTAGE_6;
    } else if (high >= board_config.pilot.levels[BOARD_CFG_PILOT_LEVEL_3]) {
        *up_voltage = PILOT_VOLTAGE_3;
    } else {
        *up_voltage = PILOT_VOLTAGE_1;
    }

    *down_voltage_n12 = low <= board_config.pilot.levels[BOARD_CFG_PILOT_LEVEL_N12];

    ESP_LOGV(TAG, "Up voltage %d", *up_voltage);
    ESP_LOGV(TAG, "Down voltage below 12V %d", *down_voltage_n12);
}
