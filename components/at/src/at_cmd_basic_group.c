#include <freertos/FreeRTOS.h>
#include <string.h>

#include "at.h"
#include "cat.h"

static void restart_func(void* arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
    vTaskDelete(NULL);
}

static void timeout_restart(void)
{
    xTaskCreate(restart_func, "restart_task", 2 * 1024, NULL, 10, NULL);
}

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

static cat_return_state cmd_rst_run(const struct cat_command* cmd)
{
    timeout_restart();

    return CAT_RETURN_STATE_OK;
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
        .name = "+RST",
        .run = cmd_rst_run,
    },
};

struct cat_command_group at_cmd_basic_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
