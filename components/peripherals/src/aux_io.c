#include "aux_io.h"

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>

#include "adc.h"
#include "board_config.h"

void aux_init(void)
{
    // digital inputs

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 0,
    };

    for (int i = 0; i < BOARD_CFG_AUX_IN_COUNT; i++) {
        if (board_cfg_is_aux_in(board_config, i)) {
            io_conf.pin_bit_mask |= BIT64(board_config.aux_in[i].gpio);
        }
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    // digital outputs

    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 0;

    for (int i = 0; i < BOARD_CFG_AUX_OUT_COUNT; i++) {
        if (board_cfg_is_aux_out(board_config, i)) {
            io_conf.pin_bit_mask |= BIT64(board_config.aux_out[i].gpio);
        }
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    // analog inputs

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    for (int i = 0; i < BOARD_CFG_AUX_ANALOG_IN_COUNT; i++) {
        if (board_cfg_is_aux_analog_in(board_config, i)) {
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.aux_analog_in[i].adc_channel, &config));
        }
    }
}

esp_err_t aux_read(const char* name, bool* value)
{
    for (int i = 0; i < BOARD_CFG_AUX_IN_COUNT; i++) {
        if (board_cfg_is_aux_in(board_config, i) && !strcmp(name, board_config.aux_in[i].name)) {
            *value = gpio_get_level(board_config.aux_in[i].gpio) == 1;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t aux_write(const char* name, bool value)
{
    for (int i = 0; i < BOARD_CFG_AUX_OUT_COUNT; i++) {
        if (board_cfg_is_aux_out(board_config, i) && !strcmp(name, board_config.aux_out[i].name)) {
            return gpio_set_level(board_config.aux_out[i].gpio, value);
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t aux_analog_read(const char* name, int* value)
{
    for (int i = 0; i < BOARD_CFG_AUX_ANALOG_IN_COUNT; i++) {
        if (board_cfg_is_aux_analog_in(board_config, i) && !strcmp(board_config.aux_analog_in[i].name, name)) {
            int raw = 0;
            esp_err_t ret = adc_oneshot_read(adc_handle, board_config.aux_analog_in[i].adc_channel, &raw);
            if (ret == ESP_OK) {
                return adc_cali_raw_to_voltage(adc_cali_handle, raw, value);
            } else {
                return ret;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}