#include <string.h>
#include <ctype.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"

#include "board_config.h"

static const char* TAG = "board_config";

board_config_t board_config;

bool atob(const char* value)
{
    return value[0] == 'y';
}

board_config_energy_meter_t atoem(const char* value)
{
    if (!strcmp(value, "cur")) {
        return BOARD_CONFIG_ENERGY_METER_CUR;
    }
    if (!strcmp(value, "cur_vlt")) {
        return BOARD_CONFIG_ENERGY_METER_CUR_VLT;
    }
    return BOARD_CONFIG_ENERGY_METER_NONE;
}

board_config_serial_t atoser(const char* value)
{
    if (!strcmp(value, "uart")) {
        return BOARD_CONFIG_SERIAL_UART;
    }
    if (!strcmp(value, "rs485")) {
        return BOARD_CONFIG_SERIAL_RS485;
    }
    return BOARD_CONFIG_SERIAL_NONE;
}

#define SET_CONFIG_VALUE(name, prop, convert_fn)    \
    if (!strcmp(key, name)) {                       \
        board_config.prop = convert_fn(value);      \
        continue;                                   \
    }                                               \

#define SET_CONFIG_VALUE_STR(name, prop)            \
    if (!strcmp(key, name)) {                       \
        strcpy(board_config.prop, value);           \
        continue;                                   \
    }                                               \

void board_config_load()
{
    memset(&board_config, 0, sizeof(board_config_t));

    FILE* file = fopen("/cfg/board.cfg", "r");
    if (file == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
    }

    char buffer[256];

    while (fgets(buffer, 256, file)) {
        int buf_length = strlen(buffer);
        int buf_start = 0;
        while (buf_start < buf_length && isspace((unsigned char)buffer[buf_start])) {
            buf_start++;
        }
        int buf_end = buf_length;
        while (buf_end > 0 && !isgraph((unsigned char)buffer[buf_end - 1])) {
            buf_end--;
        }

        buffer[buf_end] = '\0';
        char* line = &buffer[buf_start];

        if (line[0] != '#') {
            char* saveptr;
            char* key = strtok_r(line, "=", &saveptr);
            if (key != NULL) {
                char* value = strtok_r(NULL, "=", &saveptr);
                if (value != NULL) {
                    SET_CONFIG_VALUE_STR("DEVICE_NAME", device_name);
                    SET_CONFIG_VALUE("LED_CHARGING", led_charging, atob);
                    SET_CONFIG_VALUE("LED_CHARGING_GPIO", led_charging_gpio, atoi);
                    SET_CONFIG_VALUE("LED_ERROR", led_error, atob);
                    SET_CONFIG_VALUE("LED_ERROR_GPIO", led_error_gpio, atoi);
                    SET_CONFIG_VALUE("LED_WIFI", led_wifi, atob);
                    SET_CONFIG_VALUE("LED_WIFI_GPIO", led_wifi_gpio, atoi);
                    SET_CONFIG_VALUE("BUTTION_WIFI_GPIO", button_wifi_gpio, atoi);
                    SET_CONFIG_VALUE("PILOT_PWM_GPIO", pilot_pwm_gpio, atoi);
                    SET_CONFIG_VALUE("PILOT_ADC_CHANNEL", pilot_adc_channel, atoi);
                    SET_CONFIG_VALUE("PILOT_DOWN_TRESHOLD_12", pilot_down_treshold_12, atoi);
                    SET_CONFIG_VALUE("PILOT_DOWN_TRESHOLD_9", pilot_down_treshold_9, atoi);
                    SET_CONFIG_VALUE("PILOT_DOWN_TRESHOLD_6", pilot_down_treshold_6, atoi);
                    SET_CONFIG_VALUE("PILOT_DOWN_TRESHOLD_3", pilot_down_treshold_3, atoi);
                    SET_CONFIG_VALUE("PILOT_DOWN_TRESHOLD_N12", pilot_down_treshold_n12, atoi);
                    SET_CONFIG_VALUE("PROXIMITY", proximity, atob);
                    SET_CONFIG_VALUE("PROXIMITY_ADC_CHANNEL", proximity_adc_channel, atoi);
                    SET_CONFIG_VALUE("PROXIMITY_DOWN_TRESHOLD_13", proximity_down_treshold_13, atoi);
                    SET_CONFIG_VALUE("PROXIMITY_DOWN_TRESHOLD_20", proximity_down_treshold_20, atoi);
                    SET_CONFIG_VALUE("PROXIMITY_DOWN_TRESHOLD_32", proximity_down_treshold_32, atoi);
                    SET_CONFIG_VALUE("MAX_CHARGING_CURRENT", max_charging_current, atoi);
                    SET_CONFIG_VALUE("AC_RELAY_GPIO", ac_relay_gpio, atoi);
                    SET_CONFIG_VALUE("SOCKET_LOCK", socket_lock, atob);
                    SET_CONFIG_VALUE("SOCKET_LOCK_A_GPIO", socket_lock_a_gpio, atoi);
                    SET_CONFIG_VALUE("SOCKET_LOCK_B_GPIO", socket_lock_b_gpio, atoi);
                    SET_CONFIG_VALUE("SOCKET_LOCK_DETECTION_GPIO", socket_lock_detection_gpio, atoi);
                    SET_CONFIG_VALUE("SOCKET_LOCK_DETECTION_DELAY", socket_lock_detection_delay, atoi);
                    SET_CONFIG_VALUE("SOCKET_LOCK_MIN_BREAK_TIME", socket_lock_min_break_time, atoi);
                    SET_CONFIG_VALUE("RCM", rcm, atob);
                    SET_CONFIG_VALUE("RCM_GPIO", rcm_gpio, atoi);
                    SET_CONFIG_VALUE("RCM_TEST_GPIO", rcm_test_gpio, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER", energy_meter, atoem);
                    SET_CONFIG_VALUE("ENERGY_METER_THREE_PHASES", energy_meter_three_phases, atob);
                    SET_CONFIG_VALUE("ENERGY_METER_L1_CUR_ADC_CHANNEL", energy_meter_l1_cur_adc_channel, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_L2_CUR_ADC_CHANNEL", energy_meter_l2_cur_adc_channel, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_L3_CUR_ADC_CHANNEL", energy_meter_l3_cur_adc_channel, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_CUR_SCALE", energy_meter_cur_scale, atoff);
                    SET_CONFIG_VALUE("ENERGY_METER_L1_VLT_ADC_CHANNEL", energy_meter_l1_vlt_adc_channel, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_L2_VLT_ADC_CHANNEL", energy_meter_l2_vlt_adc_channel, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_L3_VLT_ADC_CHANNEL", energy_meter_l3_vlt_adc_channel, atoi);
                    SET_CONFIG_VALUE("ENERGY_METER_VLT_SCALE", energy_meter_vlt_scale, atoff);
                    SET_CONFIG_VALUE("AUX_1", aux_1, atob);
                    SET_CONFIG_VALUE("AUX_1_GPIO", aux_1_gpio, atoi);
                    SET_CONFIG_VALUE("AUX_2", aux_2, atob);
                    SET_CONFIG_VALUE("AUX_2_GPIO", aux_2_gpio, atoi);
                    SET_CONFIG_VALUE("AUX_3", aux_3, atob);
                    SET_CONFIG_VALUE("AUX_3_GPIO", aux_3_gpio, atoi);
#if CONFIG_ESP_CONSOLE_UART_NUM != 0
                    SET_CONFIG_VALUE("SERIAL_1", serial_1, atoser);
                    SET_CONFIG_VALUE_STR("SERIAL_1_NAME", serial_1_name);
                    SET_CONFIG_VALUE("SERIAL_1_RXD_GPIO", serial_1_rxd_gpio, atoi);
                    SET_CONFIG_VALUE("SERIAL_1_TXD_GPIO", serial_1_txd_gpio, atoi);
                    SET_CONFIG_VALUE("SERIAL_1_RTS_GPIO", serial_1_rts_gpio, atoi);
#endif /* CONFIG_ESP_CONSOLE_UART_NUM != 0 */
#if CONFIG_ESP_CONSOLE_UART_NUM != 1
                    SET_CONFIG_VALUE("SERIAL_2", serial_2, atoser);
                    SET_CONFIG_VALUE_STR("SERIAL_2_NAME", serial_2_name);
                    SET_CONFIG_VALUE("SERIAL_2_RXD_GPIO", serial_2_rxd_gpio, atoi);
                    SET_CONFIG_VALUE("SERIAL_2_TXD_GPIO", serial_2_txd_gpio, atoi);
                    SET_CONFIG_VALUE("SERIAL_2_RTS_GPIO", serial_2_rts_gpio, atoi);
#endif /* CONFIG_ESP_CONSOLE_UART_NUM != 1 */
#if SOC_UART_NUM > 2
#if CONFIG_ESP_CONSOLE_UART_NUM != 2
                    SET_CONFIG_VALUE("SERIAL_3", serial_3, atoser);
                    SET_CONFIG_VALUE_STR("SERIAL_3_NAME", serial_3_name);
                    SET_CONFIG_VALUE("SERIAL_3_RXD_GPIO", serial_3_rxd_gpio, atoi);
                    SET_CONFIG_VALUE("SERIAL_3_TXD_GPIO", serial_3_txd_gpio, atoi);
                    SET_CONFIG_VALUE("SERIAL_3_RTS_GPIO", serial_3_rts_gpio, atoi);
#endif /* CONFIG_ESP_CONSOLE_UART_NUM != 2 */
#endif /* SOC_UART_NUM > 2 */
                    SET_CONFIG_VALUE("TEMP_SENSOR", temp_sensor, atob);
                    SET_CONFIG_VALUE("TEMP_SENSOR_GPIO", temp_sensor_gpio, atoi);

                    ESP_LOGE(TAG, "Unknown config value %s=%s", key, value);
                }
            }
        }
    }

    fclose(file);
}
