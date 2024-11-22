#include "board_config.h"

#include <esp_err.h>
#include <esp_log.h>

#include "board_config_parser.h"

// static const char* TAG = "board_config";

board_cfg_t board_config;

void board_config_load(void)
{
    FILE* file = fopen("/cfg/board.yaml", "r");
    if (file == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
    }

    esp_err_t ret = board_config_parse_file(file, &board_config);

    fclose(file);

    ESP_ERROR_CHECK(ret);
}
