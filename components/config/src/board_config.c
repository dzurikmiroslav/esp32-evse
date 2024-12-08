#include "board_config.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <unistd.h>

#include "board_config_parser.h"

static const char* TAG = "board_config";

#ifdef CONFIG_IDF_TARGET_ESP32
extern const char board_yaml_start[] asm("_binary_board_esp32_yaml_start");
extern const char board_yaml_end[] asm("_binary_board_esp32_yaml_end");
#endif /* CONFIG_IDF_TARGET_ESP32 */

#ifdef CONFIG_IDF_TARGET_ESP32S2
extern const char board_yaml_start[] asm("_binary_board_esp32s2_yaml_start");
extern const char board_yaml_end[] asm("_binary_board_esp32s2_yaml_end");
#endif /* CONFIG_IDF_TARGET_ESP32S2 */

#ifdef CONFIG_IDF_TARGET_ESP32S3
extern const char board_yaml_start[] asm("_binary_board_esp32s3_yaml_start");
extern const char board_yaml_end[] asm("_binary_board_esp32s3_yaml_end");
#endif /* CONFIG_IDF_TARGET_ESP32S3 */

board_cfg_t board_config;

void board_config_load(void)
{
    if (access("/storage/board.yaml", F_OK) != 0) {
        ESP_LOGI(TAG, "Creating minimal config");
        FILE* file = fopen("/storage/board.yaml", "w");
        fwrite(board_yaml_start, sizeof(char), board_yaml_end - board_yaml_start, file);
        fclose(file);
    }

    FILE* file = fopen("/storage/board.yaml", "r");
    esp_err_t ret = board_config_parse_file(file, &board_config);
    fclose(file);

    ESP_ERROR_CHECK(ret);
}
