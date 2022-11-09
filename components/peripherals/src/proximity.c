#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "proximity.h"
#include "board_config.h"


static const char* TAG = "proximity";

static esp_adc_cal_characteristics_t adc_char;

void proximity_init(void)
{
    if (board_config.proximity) {
        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.proximity_adc_channel, ADC_ATTEN_DB_11));

        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 1100, &adc_char);
    }
}

uint8_t proximity_get_max_current(void)
{
    int adc_reading = adc1_get_raw(board_config.proximity_adc_channel);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_char);
    ESP_LOGD(TAG, "Measured: %dmV", voltage);

    uint8_t current;
    if (voltage >= board_config.proximity_down_treshold_13) {
        current = 13;
    } else if (voltage >= board_config.proximity_down_treshold_20) {
        current = 20;
    } else if (voltage >= board_config.proximity_down_treshold_32) {
        current = 32;
    } else {
        current = 63;
    }

    ESP_LOGI(TAG, "Max current: %dA", current);

    return current;
}