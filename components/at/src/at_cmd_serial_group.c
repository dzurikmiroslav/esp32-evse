#include <string.h>

#include "cat.h"
#include "serial.h"
#include "vars.h"

static int vars_serial_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    serial_id_t id = cmd->name[strlen(cmd->name)] - '0';

    var_u8_1 = serial_get_mode(id);
    var_i32_1 = serial_get_baud_rate(id);
    var_u8_2 = serial_get_data_bits(id);
    var_u8_3 = serial_get_stop_bits(id);
    var_u8_4 = serial_get_parity(id);

    return 0;
}

static cat_return_state vars_serial_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
{
    serial_id_t id = cmd->name[strlen(cmd->name)] - '0';
    serial_mode_t mode = var_u8_1;
    int baud_rate = var_i32_1;
    uart_word_length_t data_bits = var_u8_2;
    uart_stop_bits_t stop_bits = var_u8_3;
    uart_parity_t parity = var_u8_4;

    return serial_set_config(id, mode, baud_rate, data_bits, stop_bits, parity) == ESP_OK ? CAT_RETURN_STATE_OK : CAT_RETURN_STATE_ERROR;
}

static struct cat_variable vars_serial[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = vars_serial_read,
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_1,
        .data_size = sizeof(var_i32_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_2,
        .data_size = sizeof(var_u8_2),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_3,
        .data_size = sizeof(var_u8_3),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_4,
        .data_size = sizeof(var_u8_4),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
};

static struct cat_command cmds[] = {
    {
        .name = "+SERIAL0",
        .var = vars_serial,
        .var_num = sizeof(vars_serial) / sizeof(vars_serial[0]),
        .need_all_vars = true,
        .write = vars_serial_write,
    },
    {
        .name = "+SERIAL1",
        .var = vars_serial,
        .var_num = sizeof(vars_serial) / sizeof(vars_serial[0]),
        .need_all_vars = true,
        .write = vars_serial_write,
    },
#if BOARD_CFG_SERIAL_COUNT > 2
    {
        .name = "+SERIAL2",
        .var = vars_serial,
        .var_num = sizeof(vars_serial) / sizeof(vars_serial[0]),
        .need_all_vars = true,
        .write = vars_serial_write,
    },
#endif
};

struct cat_command_group at_cmd_serial_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
