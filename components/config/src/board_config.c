#include "board_config.h"

#include <ctype.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <string.h>

static const char* TAG = "config";

board_cfg_t board_config;

#define SET_CONFIG_VALUE(name, prop, convert_fn) \
    if (!strcmp(key, name)) {                    \
        config.prop = convert_fn(value);   \
        continue;                                \
    }

#define SET_CONFIG_VALUE_STR(name, prop)  \
    if (!strcmp(key, name)) {             \
        strcpy(config.prop, value); \
        continue;                         \
    }

void board_config_load(void)
{
    memset(&board_config, 0, sizeof(board_cfg_t));

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
                    //                     SET_CONFIG_VALUE_STR("DEVICE_NAME", device_name);
                    //                     // SET_CONFIG_VALUE("LED_CHARGING", led_charging, atob);
                    //                     SET_CONFIG_VALUE("LED_CHARGING_GPIO", led_charging_gpio, atoi);
                    //                     // SET_CONFIG_VALUE("LED_ERROR", led_error, atob);
                    //                     SET_CONFIG_VALUE("LED_ERROR_GPIO", led_error_gpio, atoi);
                    //                     // SET_CONFIG_VALUE("LED_WIFI", led_wifi, atob);
                    //                     SET_CONFIG_VALUE("LED_WIFI_GPIO", led_wifi_gpio, atoi);
                    //                     SET_CONFIG_VALUE("BUTTON_WIFI_GPIO", button_gpio, atoi);
                    //                     SET_CONFIG_VALUE("PILOT_PWM_GPIO", pilot_gpio, atoi);
                    //                     SET_CONFIG_VALUE("PILOT_ADC_CHANNEL", pilot_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("PILOT_DOWN_THRESHOLD_12", pilot_down_threshold_12, atoi);
                    //                     SET_CONFIG_VALUE("PILOT_DOWN_THRESHOLD_9", pilot_down_threshold_9, atoi);
                    //                     SET_CONFIG_VALUE("PILOT_DOWN_THRESHOLD_6", pilot_down_threshold_6, atoi);
                    //                     SET_CONFIG_VALUE("PILOT_DOWN_THRESHOLD_3", pilot_down_threshold_3, atoi);
                    //                     SET_CONFIG_VALUE("PILOT_DOWN_THRESHOLD_N12", pilot_down_threshold_n12, atoi);
                    //                     SET_CONFIG_VALUE("PROXIMITY", proximity, atob);
                    //                     SET_CONFIG_VALUE("PROXIMITY_ADC_CHANNEL", proximity_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("PROXIMITY_DOWN_THRESHOLD_13", proximity_down_threshold_13, atoi);
                    //                     SET_CONFIG_VALUE("PROXIMITY_DOWN_THRESHOLD_20", proximity_down_threshold_20, atoi);
                    //                     SET_CONFIG_VALUE("PROXIMITY_DOWN_THRESHOLD_32", proximity_down_threshold_32, atoi);
                    //                     SET_CONFIG_VALUE("AC_RELAY_GPIO", ac_relay_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SOCKET_LOCK", socket_lock, atob);
                    //                     SET_CONFIG_VALUE("SOCKET_LOCK_A_GPIO", socket_lock_a_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SOCKET_LOCK_B_GPIO", socket_lock_b_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SOCKET_LOCK_DETECTION_GPIO", socket_lock_detection_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SOCKET_LOCK_DETECTION_DELAY", socket_lock_detection_delay, atoi);
                    //                     SET_CONFIG_VALUE("SOCKET_LOCK_MIN_BREAK_TIME", socket_lock_min_break_time, atoi);
                    //                     SET_CONFIG_VALUE("RCM", rcm, atob);
                    //                     SET_CONFIG_VALUE("RCM_GPIO", rcm_gpio, atoi);
                    //                     SET_CONFIG_VALUE("RCM_TEST_GPIO", rcm_test_gpio, atoi);
                    //                     SET_CONFIG_VALUE("ENERGY_METER", energy_meter, atoem);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_THREE_PHASES", energy_meter_three_phases, atob);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_L1_CUR_ADC_CHANNEL", energy_meter_l1_cur_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_L2_CUR_ADC_CHANNEL", energy_meter_l2_cur_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_L3_CUR_ADC_CHANNEL", energy_meter_l3_cur_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_CUR_SCALE", energy_meter_cur_scale, atoff);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_L1_VLT_ADC_CHANNEL", energy_meter_l1_vlt_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_L2_VLT_ADC_CHANNEL", energy_meter_l2_vlt_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_L3_VLT_ADC_CHANNEL", energy_meter_l3_vlt_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("ENERGY_METER_VLT_SCALE", energy_meter_vlt_scale, atoff);
                    //                     SET_CONFIG_VALUE("AUX_IN_1", aux_in_1, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_IN_1_NAME", aux_in_1_name);
                    //                     SET_CONFIG_VALUE("AUX_IN_1_GPIO", aux_in_1_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_IN_2", aux_in_2, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_IN_2_NAME", aux_in_2_name);
                    //                     SET_CONFIG_VALUE("AUX_IN_2_GPIO", aux_in_2_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_IN_3", aux_in_3, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_IN_3_NAME", aux_in_3_name);
                    //                     SET_CONFIG_VALUE("AUX_IN_3_GPIO", aux_in_3_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_IN_4", aux_in_4, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_IN_4_NAME", aux_in_4_name);
                    //                     SET_CONFIG_VALUE("AUX_IN_4_GPIO", aux_in_4_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_OUT_1", aux_out_1, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_OUT_1_NAME", aux_out_1_name);
                    //                     SET_CONFIG_VALUE("AUX_OUT_1_GPIO", aux_out_1_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_OUT_2", aux_out_2, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_OUT_2_NAME", aux_out_2_name);
                    //                     SET_CONFIG_VALUE("AUX_OUT_2_GPIO", aux_out_2_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_OUT_3", aux_out_3, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_OUT_3_NAME", aux_out_3_name);
                    //                     SET_CONFIG_VALUE("AUX_OUT_3_GPIO", aux_out_3_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_OUT_4", aux_out_4, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_OUT_4_NAME", aux_out_4_name);
                    //                     SET_CONFIG_VALUE("AUX_OUT_4_GPIO", aux_out_4_gpio, atoi);
                    //                     SET_CONFIG_VALUE("AUX_AIN_1", aux_ain_1, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_AIN_1_NAME", aux_ain_1_name);
                    //                     SET_CONFIG_VALUE("AUX_AIN_1_ADC_CHANNEL", aux_ain_1_adc_channel, atoi);
                    //                     SET_CONFIG_VALUE("AUX_AIN_2", aux_ain_2, atob);
                    //                     SET_CONFIG_VALUE_STR("AUX_AIN_2_NAME", aux_ain_2_name);
                    //                     SET_CONFIG_VALUE("AUX_AIN_2_ADC_CHANNEL", aux_ain_2_adc_channel, atoi);
                    // #if CONFIG_ESP_CONSOLE_NONE || CONFIG_ESP_CONSOLE_UART_NUM != 0
                    //                     SET_CONFIG_VALUE("SERIAL_1", serial_1, atoser);
                    //                     SET_CONFIG_VALUE_STR("SERIAL_1_NAME", serial_1_name);
                    //                     SET_CONFIG_VALUE("SERIAL_1_RXD_GPIO", serial_1_rxd_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SERIAL_1_TXD_GPIO", serial_1_txd_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SERIAL_1_RTS_GPIO", serial_1_rts_gpio, atoi);
                    // #endif /* CONFIG_ESP_CONSOLE_NONE || CONFIG_ESP_CONSOLE_UART_NUM != 0 */
                    // #if CONFIG_ESP_CONSOLE_NONE || CONFIG_ESP_CONSOLE_UART_NUM != 1
                    //                     SET_CONFIG_VALUE("SERIAL_2", serial_2, atoser);
                    //                     SET_CONFIG_VALUE_STR("SERIAL_2_NAME", serial_2_name);
                    //                     SET_CONFIG_VALUE("SERIAL_2_RXD_GPIO", serial_2_rxd_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SERIAL_2_TXD_GPIO", serial_2_txd_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SERIAL_2_RTS_GPIO", serial_2_rts_gpio, atoi);
                    // #endif /* CONFIG_ESP_CONSOLE_NONE || CONFIG_ESP_CONSOLE_UART_NUM != 1 */
                    // #if SOC_UART_NUM > 2
                    // #if CONFIG_ESP_CONSOLE_NONE || CONFIG_ESP_CONSOLE_UART_NUM != 2
                    //                     SET_CONFIG_VALUE("SERIAL_3", serial_3, atoser);
                    //                     SET_CONFIG_VALUE_STR("SERIAL_3_NAME", serial_3_name);
                    //                     SET_CONFIG_VALUE("SERIAL_3_RXD_GPIO", serial_3_rxd_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SERIAL_3_TXD_GPIO", serial_3_txd_gpio, atoi);
                    //                     SET_CONFIG_VALUE("SERIAL_3_RTS_GPIO", serial_3_rts_gpio, atoi);
                    // #endif /* CONFIG_ESP_CONSOLE_NONE || CONFIG_ESP_CONSOLE_UART_NUM != 2 */
                    // #endif /* SOC_UART_NUM > 2 */
                    //                     SET_CONFIG_VALUE("ONEWIRE", onewire, atob);
                    //                     SET_CONFIG_VALUE("ONEWIRE_GPIO", onewire_gpio, atoi);
                    //                     SET_CONFIG_VALUE("ONEWIRE_TEMP_SENSOR", onewire_temp_sensor, atob);

                    ESP_LOGE(TAG, "Unknown config value %s=%s", key, value);
                }
            }
        }
    }

    fclose(file);
}
