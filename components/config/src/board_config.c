#include "board_config.h"

#include <esp_err.h>
#include <esp_log.h>
#include <stdio.h>
#include <unistd.h>

#include "board_config_parser.h"

#define BOARD_CONFIG_YAML         "/usr/board.yaml"
#define BOARD_CONFIG_INVALID_YAML "/usr/board_invalid.yaml"

static const char* TAG = "board_config";

extern const char board_yaml_start[] asm("_binary_board_" CONFIG_IDF_TARGET "_yaml_start");
extern const char board_yaml_end[] asm("_binary_board_" CONFIG_IDF_TARGET "_yaml_end");

board_cfg_t board_config;

void board_config_load(bool reset)
{
    if (reset) {
        ESP_LOGW(TAG, "Removing config");
        rename(BOARD_CONFIG_YAML, BOARD_CONFIG_INVALID_YAML);
    }

    FILE* file = fopen(BOARD_CONFIG_YAML, "r");
    if (!file) {
        ESP_LOGI(TAG, "Creating minimal config");
        file = fopen(BOARD_CONFIG_YAML, "w");
        fwrite(board_yaml_start, sizeof(char), board_yaml_end - board_yaml_start, file);
        file = freopen(BOARD_CONFIG_YAML, "r", file);
    }

    esp_err_t ret = board_config_parse_file(file, &board_config);
    fclose(file);

#ifdef CONFIG_ESP_CONSOLE_UART
    board_config.serials[CONFIG_ESP_CONSOLE_UART_NUM].type = BOARD_CFG_SERIAL_TYPE_NONE;
#endif /*CONFIG_ESP_CONSOLE_UART*/

    ESP_ERROR_CHECK(ret);
}
