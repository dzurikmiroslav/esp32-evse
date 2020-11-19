#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_err.h"

#include "board_config.h"

static const char *TAG = "board_config";

board_config_t board_config;

bool atob(const char *value)
{
    return value[0] == 'y';
}

board_config_energy_meter_t atoem(const char *value)
{
    if (!strcmp(value, "internal")) {
        return BOARD_CONFIG_ENERGY_METER_INTERNAL;
    }
    if (!strcmp(value, "external_pulse")) {
        return BOARD_CONFIG_ENERGY_METER_EXTERNAL_PULSE;
    }
    return BOARD_CONFIG_ENERGY_METER_NONE;
}

#define SET_CONFIG_VALUE(name, prop, convert_fn)    \
    if (!strcmp(key, name)) {                       \
        board_config.prop = convert_fn(value);      \
        continue;                                   \
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

        if (line[0] != '#') {
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
                    SET_CONFIG_VALUE("MAX_CHARGING_CURRENT", max_charging_current, atoi);
                    SET_CONFIG_VALUE("AC_RELAY_GPIO", ac_relay_gpio, atoi);
                    SET_CONFIG_VALUE("CABLE_LOCK", cable_lock, atob);
                    SET_CONFIG_VALUE("CABLE_LOCK_B_GPIO", cable_lock_b_gpio, atoi);
                    SET_CONFIG_VALUE("CABLE_LOCK_R_GPIO", cable_lock_r_gpio, atoi);
                    SET_CONFIG_VALUE("CABLE_LOCK_W_GPIO", cable_lock_w_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER", energy_meter, atoem);
                    SET_CONFIG_VALUE("ENERGY_METER_INTERNAL_NUM_PHASES", energy_meter_internal_num_phases, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_INTERNAL_L1_CUR_GPIO", energy_meter_internal_l1_cur_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_INTERNAL_L2_CUR_GPIO", energy_meter_internal_l2_cur_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_INTERNAL_L3_CUR_GPIO", energy_meter_internal_l3_cur_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_INTERNAL_L1_VTL_GPIO", energy_meter_internal_l1_vtl_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_INTERNAL_L2_VTL_GPIO", energy_meter_internal_l2_vtl_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_INTERNAL_L3_VTL_GPIO", energy_meter_internal_l3_vtl_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_EXTERNAL_PULSE_GPIO", energy_meter_external_pulse_gpio, atoi);

                    ESP_LOGW(TAG, "Unknown config value %s", value);
                }
            }
        }
    }

    fclose(file);
}
