#include <string.h>

#include "at.h"
#include "wifi.h"

void at_task_context_init(at_task_context_t* context, struct cat_object* at)
{
    context->at = at;
    context->echo = false;
    context->wifi_scan_ap_list = NULL;

    context->subscribe_list = (at_subscribe_list_t*)malloc(sizeof(at_subscribe_list_t));
    SLIST_INIT(context->subscribe_list);
}

void at_task_context_clean(at_task_context_t* context)
{
    if (context->wifi_scan_ap_list) {
        wifi_scan_aps_free(context->wifi_scan_ap_list);
    }

    while (!SLIST_EMPTY(context->subscribe_list)) {
        at_subscribe_entry_t* entry = SLIST_FIRST(context->subscribe_list);

        SLIST_REMOVE_HEAD(context->subscribe_list, entries);

        free((void*)entry);
    }

    free((void*)context->subscribe_list);
}

void at_handle_subscription(at_task_context_t* context)
{
    TickType_t now = xTaskGetTickCount();

    at_subscribe_entry_t* entry;
    SLIST_FOREACH (entry, context->subscribe_list, entries) {
        if ((now - entry->last_tick) >= pdMS_TO_TICKS(entry->period)) {
            cat_trigger_unsolicited_read(context->at, entry->command);
            entry->last_tick = now;
        }
    }
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
    if (strlen(command_name) == 0) {
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