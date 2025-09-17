#include <freertos/FreeRTOS.h>
#include <string.h>

#include "at.h"
#include "at_task_context.h"
#include "cat.h"
#include "vars.h"

static cat_return_state cmd_cmd_run(const struct cat_command* cmd)
{
    return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

static cat_return_state cmd_echo_run(const struct cat_command* cmd)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    context->echo = cmd->name[strlen(cmd->name) - 1] == '1';

    return CAT_RETURN_STATE_OK;
}

static struct cat_variable vars_subscribe[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_WRITE_ONLY,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u32_1,
        .data_size = sizeof(var_u32_1),
        .access = CAT_VAR_ACCESS_WRITE_ONLY,
    },
};

static cat_return_state vars_subscribe_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    return at_task_context_subscribe(context, var_str32_1, var_u32_1) ? CAT_RETURN_STATE_OK : CAT_RETURN_STATE_ERROR;
}

static struct cat_variable vars_unsubscribe[] = { {
    .type = CAT_VAR_BUF_STRING,
    .data = var_str32_1,
    .data_size = sizeof(var_str32_1),
    .access = CAT_VAR_ACCESS_WRITE_ONLY,
} };

static cat_return_state vars_unsubscribe_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    return at_task_context_unsubscribe(context, var_str32_1) ? CAT_RETURN_STATE_OK : CAT_RETURN_STATE_ERROR;
}

static struct cat_command cmds[] = {
    {
        .name = "+CMD",
        .run = cmd_cmd_run,
    },
    {
        .name = "E0",
        .run = cmd_echo_run,
    },
    {
        .name = "E1",
        .run = cmd_echo_run,
    },
    {
        .name = "+SUB",
        .var = vars_subscribe,
        .var_num = sizeof(vars_subscribe) / sizeof(vars_subscribe[0]),
        .write = vars_subscribe_write,
    },
    {
        .name = "+UNSUB",
        .var = vars_unsubscribe,
        .var_num = sizeof(vars_unsubscribe) / sizeof(vars_unsubscribe[0]),
        .write = vars_unsubscribe_write,
        .need_all_vars = false,
    },
};

struct cat_command_group at_cmd_basic_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
