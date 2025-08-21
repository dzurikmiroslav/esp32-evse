#include <string.h>

#include "at.h"
#include "board_config.h"
#include "cat.h"
#include "vars.h"

static struct cat_variable vars_device_name[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = board_config.device_name,
        .data_size = sizeof(board_config.device_name),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static int var_socket_lock_read(const struct cat_variable* var)
{
    var_u8_1 = board_cfg_is_socket_lock(board_config);

    return 0;
}

static struct cat_variable vars_socket_lock[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_socket_lock_read,
    },
};

static struct cat_variable vars_socket_lock_min_break_time[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &board_config.socket_lock.min_break_time,
        .data_size = sizeof(board_config.socket_lock.min_break_time),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static int var_proximity_read(const struct cat_variable* var)
{
    var_u8_1 = board_cfg_is_proximity(board_config);

    return 0;
}

static struct cat_variable vars_proximity[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_proximity_read,
    },
};

static int var_temperature_sensor_read(const struct cat_variable* var)
{
    var_u8_1 = board_cfg_is_onewire(board_config) && board_config.onewire.temp_sensor;

    return 0;
}

static struct cat_variable vars_temperature_sensor[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_temperature_sensor_read,
    },
};

static int vars_energy_meter_read(const struct cat_variable* var)
{
    var_u8_1 = 0;
    var_u8_2 = 0;
    if (board_cfg_is_energy_meter_cur(board_config)) {
        if (board_cfg_is_energy_meter_vlt(board_config)) {
            var_u8_1 = 2;
            var_u8_2 = board_cfg_is_energy_meter_cur_3p(board_config) && board_cfg_is_energy_meter_vlt_3p(board_config);
        } else {
            var_u8_1 = 1;
            var_u8_2 = board_cfg_is_energy_meter_vlt_3p(board_config);
        }
    }

    return 0;
}

static struct cat_variable vars_energy_meter[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = vars_energy_meter_read,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_2,
        .data_size = sizeof(var_u8_2),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static bool pop_next_serial(at_task_context_t* context)
{
    while (context->serial_index < BOARD_CFG_SERIAL_COUNT && board_config.serials[context->serial_index].type == BOARD_CFG_SERIAL_TYPE_NONE) {
        context->serial_index++;
    }

    if (context->serial_index == BOARD_CFG_SERIAL_COUNT) {
        // workaround for emit one more CAT_RETURN_STATE_DATA_NEXT for flush output
        context->serial_index++;

        return true;
    } else if (context->serial_index < BOARD_CFG_SERIAL_COUNT) {
        var_u8_1 = board_config.serials[context->serial_index].type;
        strcpy(var_str32_1, board_config.serials[context->serial_index].name);
        context->serial_index++;

        return true;
    } else {
        return false;
    }
}

static struct cat_variable vars_serial[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static cat_return_state serial_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size);

// unsolicited command
static struct cat_command cmd_serial_read = {
    .name = "+SERIALS",
    .read = serial_read,
    .var = vars_serial,
    .var_num = sizeof(vars_serial) / sizeof(vars_serial[0]),
};

// unsolicited read callback handler
static cat_return_state serial_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    if (pop_next_serial(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_serial_read);
        return CAT_RETURN_STATE_DATA_NEXT;
    } else {
        return CAT_RETURN_STATE_HOLD_EXIT_OK;
    }
}

static cat_return_state serials_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);
    context->serial_index = 0;

    if (pop_next_serial(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_serial_read);
        return CAT_RETURN_STATE_HOLD;
    } else {
        return CAT_RETURN_STATE_OK;
    }
}

// aux inputs

static bool pop_next_aux_input(at_task_context_t* context)
{
    while (context->aux_input_index < BOARD_CFG_AUX_INPUT_COUNT && !board_cfg_is_aux_input(board_config, context->aux_input_index)) {
        context->aux_input_index++;
    }

    if (context->aux_input_index == BOARD_CFG_AUX_INPUT_COUNT) {
        // workaround for emit one more CAT_RETURN_STATE_DATA_NEXT for flush output
        context->aux_input_index++;

        return true;
    } else if (context->aux_input_index < BOARD_CFG_AUX_INPUT_COUNT) {
        strcpy(var_str32_1, board_config.aux.inputs[context->aux_input_index].name);
        context->aux_input_index++;

        return true;
    } else {
        return false;
    }
}

static struct cat_variable vars_aux_input[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static cat_return_state aux_input_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size);

// unsolicited command
static struct cat_command cmd_aux_input_read = {
    .name = "+AUXINPUTS",
    .read = aux_input_read,
    .var = vars_aux_input,
    .var_num = sizeof(vars_aux_input) / sizeof(vars_aux_input[0]),
};

// unsolicited read callback handler
static cat_return_state aux_input_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    if (pop_next_aux_input(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_aux_input_read);
        return CAT_RETURN_STATE_DATA_NEXT;
    } else {
        return CAT_RETURN_STATE_HOLD_EXIT_OK;
    }
}

static cat_return_state aux_inputs_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);
    context->aux_input_index = 0;

    if (pop_next_aux_input(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_aux_input_read);
        return CAT_RETURN_STATE_HOLD;
    } else {
        return CAT_RETURN_STATE_OK;
    }
}

// aux outputs

static bool pop_next_aux_output(at_task_context_t* context)
{
    while (context->aux_output_index < BOARD_CFG_AUX_INPUT_COUNT && !board_cfg_is_aux_output(board_config, context->aux_output_index)) {
        context->aux_output_index++;
    }

    if (context->aux_output_index == BOARD_CFG_AUX_INPUT_COUNT) {
        // workaround for emit one more CAT_RETURN_STATE_DATA_NEXT for flush output
        context->aux_output_index++;

        return true;
    } else if (context->aux_output_index < BOARD_CFG_AUX_INPUT_COUNT) {
        strcpy(var_str32_1, board_config.aux.outputs[context->aux_output_index].name);
        context->aux_output_index++;

        return true;
    } else {
        return false;
    }
}

static struct cat_variable vars_aux_output[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static cat_return_state aux_output_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size);

// unsolicited command
static struct cat_command cmd_aux_output_read = {
    .name = "+AUXOUPUTS",
    .read = aux_output_read,
    .var = vars_aux_output,
    .var_num = sizeof(vars_aux_output) / sizeof(vars_aux_output[0]),
};

// unsolicited read callback handler
static cat_return_state aux_output_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    if (pop_next_aux_output(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_aux_output_read);
        return CAT_RETURN_STATE_DATA_NEXT;
    } else {
        return CAT_RETURN_STATE_HOLD_EXIT_OK;
    }
}

static cat_return_state aux_outputs_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);
    context->aux_output_index = 0;

    if (pop_next_aux_output(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_aux_output_read);
        return CAT_RETURN_STATE_HOLD;
    } else {
        return CAT_RETURN_STATE_OK;
    }
}

// aux analog inputs

static bool pop_next_aux_analog_input(at_task_context_t* context)
{
    while (context->aux_analog_input_index < BOARD_CFG_AUX_ANALOG_INPUT_COUNT && !board_cfg_is_aux_analog_input(board_config, context->aux_analog_input_index)) {
        context->aux_analog_input_index++;
    }

    if (context->aux_analog_input_index == BOARD_CFG_AUX_ANALOG_INPUT_COUNT) {
        // workaround for emit one more CAT_RETURN_STATE_DATA_NEXT for flush output
        context->aux_analog_input_index++;

        return true;
    } else if (context->aux_analog_input_index < BOARD_CFG_AUX_ANALOG_INPUT_COUNT) {
        strcpy(var_str32_1, board_config.aux.analog_inputs[context->aux_analog_input_index].name);
        context->aux_analog_input_index++;

        return true;
    } else {
        return false;
    }
}

static struct cat_variable vars_aux_analog_input[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static cat_return_state aux_analog_input_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size);

// unsolicited command
static struct cat_command cmd_aux_analog_input_read = {
    .name = "+AUXANALOGINPUTS",
    .read = aux_analog_input_read,
    .var = vars_aux_analog_input,
    .var_num = sizeof(vars_aux_analog_input) / sizeof(vars_aux_analog_input[0]),
};

// unsolicited read callback handler
static cat_return_state aux_analog_input_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    if (pop_next_aux_analog_input(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_aux_analog_input_read);
        return CAT_RETURN_STATE_DATA_NEXT;
    } else {
        return CAT_RETURN_STATE_HOLD_EXIT_OK;
    }
}

static cat_return_state aux_analog_inputs_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);
    context->aux_analog_input_index = 0;

    if (pop_next_aux_analog_input(context)) {
        cat_trigger_unsolicited_read(context->at, &cmd_aux_analog_input_read);
        return CAT_RETURN_STATE_HOLD;
    } else {
        return CAT_RETURN_STATE_OK;
    }
}

static struct cat_command cmds[] = {
    {
        .name = "+DEVNAME",
        .var = vars_device_name,
        .var_num = sizeof(vars_device_name) / sizeof(vars_device_name[0]),
    },
    {
        .name = "+SOCKETLOCK",
        .var = vars_socket_lock,
        .var_num = sizeof(vars_socket_lock) / sizeof(vars_socket_lock[0]),
    },
    {
        .name = "+SOCKETLOCKMINBREAKTIME",
        .var = vars_socket_lock_min_break_time,
        .var_num = sizeof(vars_socket_lock_min_break_time) / sizeof(vars_socket_lock_min_break_time[0]),
    },
    {
        .name = "+PROXIMITY",
        .var = vars_proximity,
        .var_num = sizeof(vars_proximity) / sizeof(vars_proximity[0]),
    },
    {
        .name = "+TEMPSENSOR",
        .var = vars_temperature_sensor,
        .var_num = sizeof(vars_temperature_sensor) / sizeof(vars_temperature_sensor[0]),
    },
    {
        .name = "+EMETER",
        .var = vars_energy_meter,
        .var_num = sizeof(vars_energy_meter) / sizeof(vars_energy_meter[0]),
    },
    {
        .name = "+SERIALS",
        .var = vars_serial,
        .var_num = sizeof(vars_serial) / sizeof(vars_serial[0]),
        .read = serials_read,
    },
    {
        .name = "+AUXINPUTS",
        .var = vars_aux_input,
        .var_num = sizeof(vars_aux_input) / sizeof(vars_aux_input[0]),
        .read = aux_inputs_read,
    },
    {
        .name = "+AUXOUTPUTS",
        .var = vars_aux_output,
        .var_num = sizeof(vars_aux_output) / sizeof(vars_aux_output[0]),
        .read = aux_outputs_read,
    },
    {
        .name = "+AUXANALOGINPUTS",
        .var = vars_aux_analog_input,
        .var_num = sizeof(vars_aux_analog_input) / sizeof(vars_aux_analog_input[0]),
        .read = aux_analog_inputs_read,
    },
};

struct cat_command_group at_cmd_board_config_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
