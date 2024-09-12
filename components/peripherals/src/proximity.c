#include "proximity.h"

#include <esp_log.h>

#include "adc.h"
#include "board_config.h"

static const char* TAG = "proximity";

void proximity_init(void)
{
    if (board_config.proximity) {
        adc_oneshot_chan_cfg_t config = {
            .bitwidth = ADC_BITWIDTH_DEFAULT,
            .atten = ADC_ATTEN_DB_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.proximity_adc_channel, &config));
    }
}

uint8_t proximity_get_max_current(void)
{
    int voltage;
    adc_oneshot_read(adc_handle, board_config.proximity_adc_channel, &voltage);
    adc_cali_raw_to_voltage(adc_cali_handle, voltage, &voltage);

    ESP_LOGD(TAG, "Measured: %dmV", voltage);

    uint8_t current;
    if (voltage >= board_config.proximity_down_threshold_13) {
        current = 13;
    } else if (voltage >= board_config.proximity_down_threshold_20) {
        current = 20;
    } else if (voltage >= board_config.proximity_down_threshold_32) {
        current = 32;
    } else {
        current = 63;
    }

    ESP_LOGI(TAG, "Max current: %dA", current);

    return current;
}