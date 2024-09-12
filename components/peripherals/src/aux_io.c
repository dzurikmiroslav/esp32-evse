#include "aux_io.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>

#include "adc.h"
#include "board_config.h"

#define MAX_AUX_IN  4
#define MAX_AUX_OUT 4
#define MAX_AUX_AIN 4

// static const char* TAG = "aux";

static int aux_in_count = 0;
static int aux_out_count = 0;
static int aux_ain_count = 0;

static struct aux_gpio_s {
    gpio_num_t gpio;
    const char* name;
} aux_in[MAX_AUX_IN], aux_out[MAX_AUX_OUT];

static struct aux_adc_s {
    adc_channel_t adc;
    const char* name;
} aux_ain[MAX_AUX_AIN];

void aux_init(void)
{
    // IN

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 0,
    };

    if (board_config.aux_in_1) {
        aux_in[aux_in_count].gpio = board_config.aux_in_1_gpio;
        aux_in[aux_in_count].name = board_config.aux_in_1_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_in_1_gpio);
        aux_in_count++;
    }

    if (board_config.aux_in_2) {
        aux_in[aux_in_count].gpio = board_config.aux_in_2_gpio;
        aux_in[aux_in_count].name = board_config.aux_in_2_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_in_2_gpio);
        aux_in_count++;
    }

    if (board_config.aux_in_3) {
        aux_in[aux_in_count].gpio = board_config.aux_in_3_gpio;
        aux_in[aux_in_count].name = board_config.aux_in_3_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_in_3_gpio);
        aux_in_count++;
    }

    if (board_config.aux_in_4) {
        aux_in[aux_in_count].gpio = board_config.aux_in_4_gpio;
        aux_in[aux_in_count].name = board_config.aux_in_4_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_in_4_gpio);
        aux_in_count++;
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    // OUT

    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 0;

    if (board_config.aux_out_1) {
        aux_out[aux_out_count].gpio = board_config.aux_out_1_gpio;
        aux_out[aux_out_count].name = board_config.aux_out_1_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_out_1_gpio);
        aux_out_count++;
    }

    if (board_config.aux_out_2) {
        aux_out[aux_out_count].gpio = board_config.aux_out_2_gpio;
        aux_out[aux_out_count].name = board_config.aux_out_2_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_out_2_gpio);
        aux_out_count++;
    }

    if (board_config.aux_out_3) {
        aux_out[aux_out_count].gpio = board_config.aux_out_3_gpio;
        aux_out[aux_out_count].name = board_config.aux_out_3_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_out_3_gpio);
        aux_out_count++;
    }

    if (board_config.aux_out_4) {
        aux_out[aux_out_count].gpio = board_config.aux_out_4_gpio;
        aux_out[aux_out_count].name = board_config.aux_out_4_name;
        io_conf.pin_bit_mask |= BIT64(board_config.aux_out_4_gpio);
        aux_out_count++;
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    // AIN

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    if (board_config.aux_ain_1) {
        aux_ain[aux_ain_count].adc = board_config.aux_ain_1_adc_channel;
        aux_ain[aux_ain_count].name = board_config.aux_out_1_name;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.aux_ain_1_adc_channel, &config));
        aux_ain_count++;
    }

    if (board_config.aux_ain_2) {
        aux_ain[aux_ain_count].adc = board_config.aux_ain_2_adc_channel;
        aux_ain[aux_ain_count].name = board_config.aux_out_2_name;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.aux_ain_2_adc_channel, &config));
        aux_ain_count++;
    }
}

esp_err_t aux_read(const char* name, bool* value)
{
    for (int i = 0; i < aux_in_count; i++) {
        if (strcmp(aux_in[i].name, name) == 0) {
            *value = gpio_get_level(aux_in[i].gpio) == 1;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t aux_write(const char* name, bool value)
{
    for (int i = 0; i < aux_out_count; i++) {
        if (strcmp(aux_out[i].name, name) == 0) {
            return gpio_set_level(aux_out[i].gpio, value);
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t aux_analog_read(const char* name, int* value)
{
    for (int i = 0; i < aux_ain_count; i++) {
        if (strcmp(aux_ain[i].name, name) == 0) {
            int raw = 0;
            esp_err_t ret = adc_oneshot_read(adc_handle, aux_ain[i].adc, &raw);
            if (ret == ESP_OK) {
                return adc_cali_raw_to_voltage(adc_cali_handle, raw, value);
            } else {
                return ret;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}