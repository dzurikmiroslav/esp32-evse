#include "at_task.h"

#include <unistd.h>

#include "at.h"

#define BUF_SIZE  256
#define READY_MSG "\n\nRDY\n"

static int write_char(char ch)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    return write(context->fd, &ch, 1);
}

static int read_char(char* ch)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    int readed;
    if (context->has_input_char) {
        *ch = context->input_char;
        context->has_input_char = false;
        readed = 1;
    } else {
        readed = read(context->fd, ch, 1) > 0 ? 1 : 0;
    }

    if (readed > 0 && context->echo) {
        write(context->fd, ch, 1);
        fsync(context->fd);
    }

    return readed;
}

static void handle_subscription(at_task_context_t* context)
{
    TickType_t now = xTaskGetTickCount();

    at_subscribe_entry_t* entry;
    SLIST_FOREACH (entry, context->subscribe_list, entries) {
        if ((now - entry->last_tick) >= pdMS_TO_TICKS(entry->period)) {
            cat_trigger_unsolicited_read(context->at, entry->command);
            entry->last_tick = now;

            // this prevents overflow of unsolicited buffer
            while (cat_service(context->at) != 0) {
            };
        }
    }
}

static void at_task_func(void* param)
{
    int fd = (int)param;

    uint8_t buf[BUF_SIZE];
    struct cat_command_group* cmd_desc[] = AT_CMD_GROUPS;

    struct cat_descriptor desc = {
        .cmd_group = cmd_desc,
        .cmd_group_num = sizeof(cmd_desc) / sizeof(cmd_desc[0]),
        .buf = buf,
        .buf_size = sizeof(buf),
    };

    struct cat_io_interface iface = {
        .read = read_char,
        .write = write_char,
    };

    struct cat_object at;
    cat_init(&at, &desc, &iface, NULL);

    at_task_context_t context = {
        .at = &at,
        .echo = false,
        .wifi_scan_ap_list = NULL,
        .has_input_char = false,
        .fd = fd,
    };
    context.subscribe_list = (at_subscribe_list_t*)malloc(sizeof(at_subscribe_list_t));
    SLIST_INIT(context.subscribe_list);

    vTaskSetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX, &context);

    write(fd, READY_MSG, sizeof(READY_MSG));

    while (true) {
        struct timeval tv = {
            .tv_sec = 0,
            .tv_usec = 250 * 1000,
        };
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(fd, &read_set);

        if (select(fd + 1, &read_set, NULL, NULL, &tv) > 0) {
            if (FD_ISSET(fd, &read_set)) {
                context.has_input_char = read(fd, &context.input_char, 1) > 0;
                if (context.has_input_char) {
                    while (cat_service(&at) != 0) {
                    };
                }
            }
        }

        handle_subscription(&context);
    }
}

TaskHandle_t at_task_start(int fd)
{
    TaskHandle_t handle = NULL;
    xTaskCreate(at_task_func, "at", 4 * 1024, (void*)fd, 5, &handle);
    return handle;
}

void at_task_stop(TaskHandle_t task)
{
    vTaskSuspend(task);

    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(task, AT_TASK_CONTEXT_INDEX);
    if (context->wifi_scan_ap_list) {
        wifi_scan_aps_free(context->wifi_scan_ap_list);
    }

    while (!SLIST_EMPTY(context->subscribe_list)) {
        at_subscribe_entry_t* entry = SLIST_FIRST(context->subscribe_list);

        SLIST_REMOVE_HEAD(context->subscribe_list, entries);

        free((void*)entry);
    }

    free((void*)context->subscribe_list);

    vTaskDelete(task);
}

bool at_task_context_subscribe(at_task_context_t* context, const char* command_name, uint32_t period)
{
    const struct cat_command* command = cat_search_command_by_name(context->at, command_name);
    if (command == NULL) {
        return false;
    }

    // try update existing subscription if exists
    at_subscribe_entry_t* entry;
    SLIST_FOREACH (entry, context->subscribe_list, entries) {
        if (entry->command == command) {
            entry->period = period;
            return true;
        }
    }

    entry = (at_subscribe_entry_t*)malloc(sizeof(at_subscribe_entry_t));
    entry->command = command;
    entry->last_tick = 0;
    entry->period = period;
    SLIST_INSERT_HEAD(context->subscribe_list, entry, entries);

    return true;
}

bool at_task_context_unsubscribe(at_task_context_t* context, const char* command_name)
{
    if (command_name[0] == '\0') {
        // unsubscribe all
        while (!SLIST_EMPTY(context->subscribe_list)) {
            at_subscribe_entry_t* entry = SLIST_FIRST(context->subscribe_list);

            SLIST_REMOVE_HEAD(context->subscribe_list, entries);

            free((void*)entry);
        }
    } else {
        const struct cat_command* command = cat_search_command_by_name(context->at, command_name);
        if (command == NULL) {
            return false;
        }

        at_subscribe_entry_t *entry, *tmp;
        SLIST_FOREACH_SAFE (entry, context->subscribe_list, entries, tmp) {
            if (entry->command == command) {
                SLIST_REMOVE(context->subscribe_list, entry, at_subscribe_entry_s, entries);

                free((void*)entry);
                break;
            }
        }
    }

    return true;
}