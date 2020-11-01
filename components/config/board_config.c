#include "../config/include/board_config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_err.h"

//static const char *TAG = "board_config";

board_config_t board_config;

bool atob(const char *value)
{
    return value[0] == 'y';
}

#define SET_CONFIG_VALUE(name, prop, convert_fn)    \
    if (!strcmp(key, name)) {                       \
        board_config.prop = convert_fn(value);      \
    }                                               \

void board_config_load()
{
    memset(&board_config, 0, sizeof(board_config_t));

    FILE *file = fopen("/cfg/board.cfg", "r");
    if (file == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
    }

    char buffer[256];

    while (fgets(buffer, 256, file)) {
        int buf_length = strlen(buffer);
        int buf_start = 0;
        while (buf_start < buf_length && isspace(buffer[buf_start])) {
            buf_start++;
        }
        int buf_end = buf_length;
        do {
            buf_end--;
        } while (buf_end > 0 && !isspace(buffer[buf_end]));

        buffer[buf_end] = '\0';
        char *line = &buffer[buf_start];

        char *saveptr;
        char *key = strtok_r(line, "=", &saveptr);
        if (key != NULL) {
            char *value = strtok_r(NULL, "=", &saveptr);

            if (value != NULL) {
                SET_CONFIG_VALUE("LED_CHARGING", led_charging, atob);
                SET_CONFIG_VALUE("LED_CHARGING_GPIO", led_charging_gpio, atoi);
                SET_CONFIG_VALUE("LED_ERROR", led_error, atob);
                SET_CONFIG_VALUE("LED_ERROR_GPIO", led_error_gpio, atoi);
                SET_CONFIG_VALUE("LED_WIFI", led_wifi, atob);
                SET_CONFIG_VALUE("LED_WIFI_GPIO", led_wifi_gpio, atoi);
                SET_CONFIG_VALUE("BUTTION_WIFI_GPIO", button_wifi_gpio, atoi);
                SET_CONFIG_VALUE("PILOT_PWM_GPIO", pilot_pwm_gpio, atoi);
                SET_CONFIG_VALUE("PILOT_SENS_ADC_CHANNEL", pilot_sens_adc_channel, atoi);
                SET_CONFIG_VALUE("PILOT_SENS_DOWN_TRESHOLD_12", pilot_sens_down_treshold_12, atoi);
                SET_CONFIG_VALUE("PILOT_SENS_DOWN_TRESHOLD_9", pilot_sens_down_treshold_9, atoi);
                SET_CONFIG_VALUE("PILOT_SENS_DOWN_TRESHOLD_6", pilot_sens_down_treshold_6, atoi);
                SET_CONFIG_VALUE("PILOT_SENS_DOWN_TRESHOLD_3", pilot_sens_down_treshold_3, atoi);
                SET_CONFIG_VALUE("AC_RELAY_GPIO", ac_relay_gpio, atoi);

            }
        }
    }

    fclose(file);
}
