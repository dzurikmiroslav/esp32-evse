#ifndef BOARD_CONFIG_PARSER_H_
#define BOARD_CONFIG_PARSER_H_

#include <esp_err.h>
#include <stdio.h>

#include "board_config.h"

esp_err_t board_config_parse_file(FILE* src, board_cfg_t* board_cfg);

#endif /* BOARD_CONFIG_PARSER_H_ */
