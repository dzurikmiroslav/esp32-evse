#ifndef BOARD_CONFIG_PARSER_H_
#define BOARD_CONFIG_PARSER_H_

#include <stdio.h>

#include "board_config.h"

bool board_cfg_parse_file(FILE* src, board_cfg_t* board_cfg);

#endif /* BOARD_CONFIG_PARSER_H_ */
