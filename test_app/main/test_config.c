#include <soc/uart_channel.h>
#include <sys/stat.h>
#include <unity.h>
#include <unity_fixture.h>

#include "board_config.h"
#include "board_config_parser.h"

#define BOARD_YAML "/usr/board.yaml"

#ifdef CONFIG_IDF_TARGET_ESP32
#define DEVICE_NANE       "ESP32 minimal EVSE"
#define PILOT_GPIO        33
#define PILOT_ADC_CHANNEL 7
#define AC_RELAY_GPIO     32
#endif /* CONFIG_IDF_TARGET_ESP32 */

#ifdef CONFIG_IDF_TARGET_ESP32S2
#define DEVICE_NANE       "ESP32-S2 minimal EVSE"
#define PILOT_GPIO        6
#define PILOT_ADC_CHANNEL 3
#define AC_RELAY_GPIO     5
#endif /* CONFIG_IDF_TARGET_ESP32S2 */

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define DEVICE_NANE       "ESP32-S3 minimal EVSE"
#define PILOT_GPIO        6
#define PILOT_ADC_CHANNEL 3
#define AC_RELAY_GPIO     5
#endif /* CONFIG_IDF_TARGET_ESP32S3 */

extern const char board_yaml_start[] asm("_binary_board_yaml_start");
extern const char board_yaml_end[] asm("_binary_board_yaml_end");

TEST_GROUP(config);

TEST_SETUP(config)
{}

TEST_TEAR_DOWN(config)
{}

TEST(config, minimal)
{
    remove(BOARD_YAML);

    board_config_load(false);

    TEST_ASSERT_EQUAL_STRING(DEVICE_NANE, board_config.device_name);

    TEST_ASSERT_EQUAL(-1, board_config.led_charging_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.led_error_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.led_wifi_gpio);
    TEST_ASSERT_EQUAL(0, board_config.button_gpio);

    TEST_ASSERT_EQUAL(PILOT_GPIO, board_config.pilot_gpio);
    TEST_ASSERT_EQUAL(PILOT_ADC_CHANNEL, board_config.pilot_adc_channel);
    TEST_ASSERT_EQUAL(2410, board_config.pilot_levels[0]);
    TEST_ASSERT_EQUAL(2104, board_config.pilot_levels[1]);
    TEST_ASSERT_EQUAL(1797, board_config.pilot_levels[2]);
    TEST_ASSERT_EQUAL(1491, board_config.pilot_levels[3]);
    TEST_ASSERT_EQUAL(265, board_config.pilot_levels[4]);

    TEST_ASSERT_EQUAL(AC_RELAY_GPIO, board_config.ac_relay_gpio);

    TEST_ASSERT_EQUAL(-1, board_config.socket_lock_a_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.socket_lock_b_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.socket_lock_detection_gpio);
    TEST_ASSERT_EQUAL(0, board_config.socket_lock_detection_delay);
    TEST_ASSERT_EQUAL(0, board_config.socket_lock_min_break_time);

    TEST_ASSERT_EQUAL(-1, board_config.energy_meter_cur_adc_channel[0]);
    TEST_ASSERT_EQUAL(-1, board_config.energy_meter_cur_adc_channel[1]);
    TEST_ASSERT_EQUAL(-1, board_config.energy_meter_cur_adc_channel[2]);
    TEST_ASSERT_EQUAL_FLOAT(0, board_config.energy_meter_cur_scale);
    TEST_ASSERT_EQUAL(-1, board_config.energy_meter_vlt_adc_channel[0]);
    TEST_ASSERT_EQUAL(-1, board_config.energy_meter_vlt_adc_channel[1]);
    TEST_ASSERT_EQUAL(-1, board_config.energy_meter_vlt_adc_channel[2]);
    TEST_ASSERT_EQUAL_FLOAT(0, board_config.energy_meter_vlt_scale);

    TEST_ASSERT_EQUAL(-1, board_config.rcm_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.rcm_test_gpio);

    TEST_ASSERT_EQUAL(-1, board_config.aux_inputs[0].gpio);
    TEST_ASSERT_EQUAL(-1, board_config.aux_inputs[1].gpio);
    TEST_ASSERT_EQUAL(-1, board_config.aux_inputs[2].gpio);
    TEST_ASSERT_EQUAL(-1, board_config.aux_inputs[3].gpio);

    TEST_ASSERT_EQUAL(-1, board_config.aux_outputs[0].gpio);
    TEST_ASSERT_EQUAL(-1, board_config.aux_outputs[1].gpio);
    TEST_ASSERT_EQUAL(-1, board_config.aux_outputs[2].gpio);
    TEST_ASSERT_EQUAL(-1, board_config.aux_outputs[3].gpio);

    TEST_ASSERT_EQUAL(-1, board_config.aux_analog_inputs[0].adc_channel);
    TEST_ASSERT_EQUAL(-1, board_config.aux_analog_inputs[1].adc_channel);

    TEST_ASSERT_EQUAL(BOARD_CFG_SERIAL_TYPE_UART, board_config.serials[0].type);
    TEST_ASSERT_EQUAL_STRING("UART via USB", board_config.serials[0].name);
    TEST_ASSERT_EQUAL(UART_NUM_0_RXD_DIRECT_GPIO_NUM, board_config.serials[0].rxd_gpio);
    TEST_ASSERT_EQUAL(UART_NUM_0_TXD_DIRECT_GPIO_NUM, board_config.serials[0].txd_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.serials[0].rts_gpio);

    TEST_ASSERT_EQUAL(BOARD_CFG_SERIAL_TYPE_NONE, board_config.serials[1].type);
    TEST_ASSERT_EQUAL(-1, board_config.serials[1].rxd_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.serials[1].txd_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.serials[1].rts_gpio);

    TEST_ASSERT_EQUAL(BOARD_CFG_SERIAL_TYPE_NONE, board_config.serials[2].type);
    TEST_ASSERT_EQUAL(-1, board_config.serials[2].rxd_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.serials[2].txd_gpio);
    TEST_ASSERT_EQUAL(-1, board_config.serials[2].rts_gpio);

    TEST_ASSERT_EQUAL(-1, board_config.onewire_gpio);
    TEST_ASSERT_FALSE(board_config.onewire_temp_sensor);
}

TEST(config, custom)
{
    const char* file_name = "/usr/board1.yaml";

    FILE* config_file = fopen(file_name, "w");
    fwrite(board_yaml_start, sizeof(char), board_yaml_end - board_yaml_start, config_file);

    config_file = freopen(file_name, "r", config_file);

    board_cfg_t config;
    esp_err_t ret = board_config_parse_file(config_file, &config);
    fclose(config_file);

    TEST_ASSERT_EQUAL(ret, ESP_OK);

    TEST_ASSERT_EQUAL_STRING("test", config.device_name);
    TEST_ASSERT_EQUAL(1, config.led_charging_gpio);
    TEST_ASSERT_EQUAL(2, config.led_error_gpio);
    TEST_ASSERT_EQUAL(3, config.led_wifi_gpio);
    TEST_ASSERT_EQUAL(4, config.button_gpio);

    TEST_ASSERT_EQUAL(28, config.pilot_gpio);
    TEST_ASSERT_EQUAL(1, config.pilot_adc_channel);
    TEST_ASSERT_EQUAL(110, config.pilot_levels[0]);
    TEST_ASSERT_EQUAL(120, config.pilot_levels[1]);
    TEST_ASSERT_EQUAL(130, config.pilot_levels[2]);
    TEST_ASSERT_EQUAL(140, config.pilot_levels[3]);
    TEST_ASSERT_EQUAL(150, config.pilot_levels[4]);

    TEST_ASSERT_EQUAL(27, config.socket_lock_a_gpio);
    TEST_ASSERT_EQUAL(28, config.socket_lock_b_gpio);
    TEST_ASSERT_EQUAL(29, config.socket_lock_detection_gpio);
    TEST_ASSERT_EQUAL(1500, config.socket_lock_detection_delay);
    TEST_ASSERT_EQUAL(3000, config.socket_lock_min_break_time);

    TEST_ASSERT_EQUAL(7, config.energy_meter_cur_adc_channel[0]);
    TEST_ASSERT_EQUAL(8, config.energy_meter_cur_adc_channel[1]);
    TEST_ASSERT_EQUAL(9, config.energy_meter_cur_adc_channel[2]);
    TEST_ASSERT_EQUAL_FLOAT(0.8, config.energy_meter_cur_scale);
    TEST_ASSERT_EQUAL(10, config.energy_meter_vlt_adc_channel[0]);
    TEST_ASSERT_EQUAL(11, config.energy_meter_vlt_adc_channel[1]);
    TEST_ASSERT_EQUAL(12, config.energy_meter_vlt_adc_channel[2]);
    TEST_ASSERT_EQUAL_FLOAT(0.9, config.energy_meter_vlt_scale);

    TEST_ASSERT_EQUAL(30, config.rcm_gpio);
    TEST_ASSERT_EQUAL(31, config.rcm_test_gpio);

    TEST_ASSERT_EQUAL(21, config.aux_inputs[0].gpio);
    TEST_ASSERT_EQUAL_STRING("IN1", config.aux_inputs[0].name);
    TEST_ASSERT_EQUAL(22, config.aux_inputs[1].gpio);
    TEST_ASSERT_EQUAL_STRING("IN2", config.aux_inputs[1].name);
    TEST_ASSERT_EQUAL(-1, config.aux_inputs[2].gpio);
    TEST_ASSERT_EQUAL(-1, config.aux_inputs[3].gpio);

    TEST_ASSERT_EQUAL(23, config.aux_outputs[0].gpio);
    TEST_ASSERT_EQUAL_STRING("OUT1", config.aux_outputs[0].name);
    TEST_ASSERT_EQUAL(-1, config.aux_outputs[1].gpio);
    TEST_ASSERT_EQUAL(-1, config.aux_outputs[2].gpio);
    TEST_ASSERT_EQUAL(-1, config.aux_outputs[3].gpio);

    TEST_ASSERT_EQUAL(2, config.aux_analog_inputs[0].adc_channel);
    TEST_ASSERT_EQUAL_STRING("AIN1", config.aux_analog_inputs[0].name);
    TEST_ASSERT_EQUAL(3, config.aux_analog_inputs[1].adc_channel);
    TEST_ASSERT_EQUAL_STRING("AIN2", config.aux_analog_inputs[1].name);

    TEST_ASSERT_EQUAL(BOARD_CFG_SERIAL_TYPE_UART, config.serials[0].type);
    TEST_ASSERT_EQUAL_STRING("UART via USB", config.serials[0].name);
    TEST_ASSERT_EQUAL(3, config.serials[0].rxd_gpio);
    TEST_ASSERT_EQUAL(1, config.serials[0].txd_gpio);
    TEST_ASSERT_EQUAL(-1, config.serials[0].rts_gpio);

    TEST_ASSERT_EQUAL(BOARD_CFG_SERIAL_TYPE_RS485, config.serials[1].type);
    TEST_ASSERT_EQUAL_STRING("RS485", config.serials[1].name);
    TEST_ASSERT_EQUAL(32, config.serials[1].rxd_gpio);
    TEST_ASSERT_EQUAL(25, config.serials[1].txd_gpio);
    TEST_ASSERT_EQUAL(33, config.serials[1].rts_gpio);

    TEST_ASSERT_EQUAL(BOARD_CFG_SERIAL_TYPE_NONE, config.serials[2].type);
    TEST_ASSERT_EQUAL(-1, config.serials[2].rxd_gpio);
    TEST_ASSERT_EQUAL(-1, config.serials[2].txd_gpio);
    TEST_ASSERT_EQUAL(-1, config.serials[2].rts_gpio);

    TEST_ASSERT_EQUAL(16, config.onewire_gpio);
    TEST_ASSERT_TRUE(config.onewire_temp_sensor);
}

TEST_GROUP_RUNNER(config)
{
    RUN_TEST_CASE(config, custom);
    RUN_TEST_CASE(config, minimal);
}