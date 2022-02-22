#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "nvs.h"

#include "proximity.h"
#include "board_config.h"


#define NVS_NAMESPACE                   "proximity"
#define NVS_ENABLED                     "enabled"

static const char* TAG = "proximity";

static nvs_handle nvs;

static esp_adc_cal_characteristics_t adc_char;

static bool enabled = false;

void proximity_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    if (board_config.proximity_sens) {
        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.proximity_sens_adc_channel, ADC_ATTEN_DB_11));

        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 1100, &adc_char);
    }

    uint8_t u8;
    if (nvs_get_u8(nvs, NVS_ENABLED, &u8) == ESP_OK) {
        enabled = u8;
    }
}

bool proximity_is_enabled(void)
{
    return enabled;
}

esp_err_t proximity_set_enabled(bool _enabled)
{
    ESP_LOGI(TAG, "Set enabled %d", _enabled);

    if (_enabled && !board_config.proximity_sens) {
        ESP_LOGI(TAG, "Proximity check not available");
        return ESP_ERR_NOT_SUPPORTED;
    }

    enabled = _enabled;

    nvs_set_u16(nvs, NVS_ENABLED, enabled);
    nvs_commit(nvs);

    return ESP_OK;
}

uint8_t proximity_get_max_current(void)
{
    if (!enabled) {
        return 63;
    }

    int adc_reading = adc1_get_raw(board_config.proximity_sens_adc_channel);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_char);
    ESP_LOGD(TAG, "Measured: %dmV", voltage);

    uint8_t current;
    if (voltage >= board_config.proximity_sens_down_treshold_13) {
        current = 13;
    } else if (voltage >= board_config.proximity_sens_down_treshold_20) {
        current = 20;
    } else if (voltage >= board_config.proximity_sens_down_treshold_32) {
        current = 32;
    } else {
        current = 63;
    }

    ESP_LOGI(TAG, "Max current: %dA", current);

    return current;
}