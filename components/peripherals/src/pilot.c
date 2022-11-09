#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "pilot.h"
#include "board_config.h"

#define PILOT_PWM_TIMER         LEDC_TIMER_0
#define PILOT_PWM_CHANNEL       LEDC_CHANNEL_0
#define PILOT_PWM_SPEED_MODE    LEDC_LOW_SPEED_MODE
#define PILOT_PWM_DUTY_RES      LEDC_TIMER_10_BIT
#define PILOT_PWM_MAX_DUTY      1023

#define PILOT_SAMPLES      64

static const char* TAG = "pilot";

static esp_adc_cal_characteristics_t adc_char;

static pilot_voltage_t up_voltage;

static bool down_voltage_n12;

void pilot_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = PILOT_PWM_SPEED_MODE,
        .timer_num = PILOT_PWM_TIMER,
        .duty_resolution = PILOT_PWM_DUTY_RES,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = PILOT_PWM_SPEED_MODE,
        .channel = PILOT_PWM_CHANNEL,
        .timer_sel = PILOT_PWM_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = board_config.pilot_pwm_gpio,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_stop(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, 1));

    ledc_fade_func_install(0);

    ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.pilot_adc_channel, ADC_ATTEN_DB_11));

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 1100, &adc_char);
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

    ESP_LOGI(TAG, "Set amp %dA*10 duty %d/%d", amps, duty, PILOT_PWM_MAX_DUTY);

    ledc_set_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL, duty);
    ledc_update_duty(PILOT_PWM_SPEED_MODE, PILOT_PWM_CHANNEL);
}

void pilot_measure(void)
{
    uint32_t high = 0;
    uint32_t low = 3300;

    for (int i = 0; i < 100; i++) {
        int adc_reading = adc1_get_raw(board_config.pilot_adc_channel);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_char);
        if (voltage > high) {
            high = voltage;
        } else if (voltage < low) {
            low = voltage;
        }
        ets_delay_us(100);
    }

    ESP_LOGV(TAG, "Measure: %dmV - %dmV", low, high);

    if (high >= board_config.pilot_down_treshold_12) {
        up_voltage = PILOT_VOLTAGE_12;
    } else if (high >= board_config.pilot_down_treshold_9) {
        up_voltage = PILOT_VOLTAGE_9;
    } else if (high >= board_config.pilot_down_treshold_6) {
        up_voltage = PILOT_VOLTAGE_6;
    } else if (high >= board_config.pilot_down_treshold_3) {
        up_voltage = PILOT_VOLTAGE_3;
    } else {
        up_voltage = PILOT_VOLTAGE_1;
    }

    down_voltage_n12 = low <= board_config.pilot_down_treshold_n12;

    ESP_LOGV(TAG, "Up volate %d", up_voltage);
    ESP_LOGV(TAG, "Down volate belov 12V %d", down_voltage_n12);
}

pilot_voltage_t pilot_get_up_voltage(void)
{
    return up_voltage;
}

bool pilot_is_down_voltage_n12(void)
{
    return down_voltage_n12;
}