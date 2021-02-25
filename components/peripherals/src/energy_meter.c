#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "energy_meter.h"
#include "board_config.h"

#define PILOT_PWM_TIMER         LEDC_TIMER_0
#define PILOT_PWM_CHANNEL       LEDC_CHANNEL_0
#define PILOT_PWM_SPEED_MODE    LEDC_HIGH_SPEED_MODE
#define PILOT_PWM_MAX_DUTY      1023

static const char *TAG = "energy_meter";

static esp_adc_cal_characteristics_t sens_adc_char;

void energy_meter_init(void)
{
    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_INTERNAL) {
        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_internal_l1_cur_adc_channel, ADC_ATTEN_DB_11));
        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_internal_l1_vlt_adc_channel, ADC_ATTEN_DB_11));

        if (board_config.energy_meter_internal_num_phases > 1) {
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_internal_l2_cur_adc_channel, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_internal_l2_vlt_adc_channel, ADC_ATTEN_DB_11));
        }

        if (board_config.energy_meter_internal_num_phases > 2) {
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_internal_l3_cur_adc_channel, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_internal_l3_vlt_adc_channel, ADC_ATTEN_DB_11));
        }

        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &sens_adc_char);
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_EXTERNAL_PULSE) {
        // TODO
    }
}

static void measure(uint32_t *high, uint32_t *low, adc1_channel_t channel)
{
    int adc_reading = adc1_get_raw(channel);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &sens_adc_char);
    if (voltage > *high) {
        *high = voltage;
    } else if (voltage < *low) {
        *low = voltage;
    }
}

float energy_meter_actual_power(void)
{
    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_INTERNAL) {
        uint32_t l1_cur_high = 0;
        uint32_t l1_cur_low = 3300;
        uint32_t l1_vlt_high = 0;
        uint32_t l1_vlt_low = 3300;

        uint32_t l2_cur_high = 0;
        uint32_t l2_cur_low = 3300;
        uint32_t l2_vlt_high = 0;
        uint32_t l2_vlt_low = 3300;

        uint32_t l3_cur_high = 0;
        uint32_t l3_cur_low = 3300;
        uint32_t l3_vlt_high = 0;
        uint32_t l3_vlt_low = 3300;

        // 50Hz is 20ms one period

        for (int i = 0; i < 300; i++) {
            measure(&l1_cur_high, &l1_cur_low, board_config.energy_meter_internal_l1_cur_adc_channel);
            measure(&l1_vlt_high, &l1_vlt_low, board_config.energy_meter_internal_l1_vlt_adc_channel);

            if (board_config.energy_meter_internal_num_phases > 1) {
                measure(&l2_cur_high, &l2_cur_low, board_config.energy_meter_internal_l2_cur_adc_channel);
                measure(&l2_vlt_high, &l2_vlt_low, board_config.energy_meter_internal_l2_vlt_adc_channel);
            }

            if (board_config.energy_meter_internal_num_phases > 2) {
                measure(&l3_cur_high, &l3_cur_low, board_config.energy_meter_internal_l3_cur_adc_channel);
                measure(&l3_vlt_high, &l3_vlt_low, board_config.energy_meter_internal_l3_vlt_adc_channel);
            }

            ets_delay_us(100);
        }


        ESP_LOGI(TAG, "curr %d vcc %d", board_config.energy_meter_internal_l1_cur_adc_channel, board_config.energy_meter_internal_l1_vlt_adc_channel );
        ESP_LOGI(TAG, "L1 current adc %dmV [%dmV %dmV]", l1_cur_high - l1_cur_low, l1_cur_low, l1_cur_high);
        ESP_LOGI(TAG, "L1 voltage adc %dmV [%dmV %dmV]", l1_vlt_high - l1_vlt_low, l1_vlt_low, l1_vlt_high);

        float cur = (l1_cur_high - l1_cur_low) / 2 * board_config.energy_meter_internal_cur_scale;
        float vlt = (l1_vlt_high - l1_vlt_low) / 2 * board_config.energy_meter_internal_cur_scale;
        ESP_LOGI(TAG, "L1 %fV %fA", vlt, cur);
        float power = vlt * cur;

        if (board_config.energy_meter_internal_num_phases > 1) {
            cur = (l2_cur_high - l2_cur_low) /2 * board_config.energy_meter_internal_cur_scale;
            vlt = (l2_vlt_high - l2_vlt_low)/2 * board_config.energy_meter_internal_cur_scale;
            ESP_LOGI(TAG, "L2 %fV %fA", vlt, cur);
            power += vlt * cur;
        }

        if (board_config.energy_meter_internal_num_phases > 2) {
            cur = (l3_cur_high - l3_cur_low)/2 * board_config.energy_meter_internal_cur_scale;
            vlt = (l3_vlt_high - l3_vlt_low) /2* board_config.energy_meter_internal_cur_scale;
            ESP_LOGI(TAG, "L3 %fV %fA", vlt, cur);
            power += vlt * cur;
        }

        return power;
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_EXTERNAL_PULSE) {
        // TODO
        return 0;
    }

    return 0;

}
