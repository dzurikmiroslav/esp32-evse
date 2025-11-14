#ifndef AT_H_
#define AT_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#include "cat.h"
#include "wifi.h"

#define AT_CMD_GROUPS \
    { &at_cmd_basic_group, &at_cmd_system_group, &at_cmd_evse_group, &at_cmd_energy_meter_group, &at_cmd_network_group, &at_cmd_serial_group, &at_cmd_board_config_group }

#define AT_TASK_CONTEXT_INDEX 1

/**
 * @brief AT subscribe read command entry
 *
 */
typedef struct at_subscribe_entry_s {
    const struct cat_command* command;
    uint32_t period;
    TickType_t last_tick;
    SLIST_ENTRY(at_subscribe_entry_s) entries;
} at_subscribe_entry_t;

/**
 * @brief AT subscribe read command list
 *
 * @return typedef
 */
typedef SLIST_HEAD(at_subscribe_list_t, at_subscribe_entry_s) at_subscribe_list_t;

/**
 * @brief AT command context
 *
 */
typedef struct {
    struct cat_object* at;
    bool echo;
    wifi_scan_ap_list_t* wifi_scan_ap_list;
    at_subscribe_list_t* subscribe_list;
    uint8_t serial_index;
    uint8_t aux_input_index;
    uint8_t aux_output_index;
    uint8_t aux_analog_input_index;
    char input_char;
    bool has_input_char : 1;
} at_task_context_t;

void at_task_context_init(at_task_context_t* context, struct cat_object* at);

void at_task_context_clean(at_task_context_t* context);

void at_handle_subscription(at_task_context_t* context);

extern struct cat_command_group at_cmd_basic_group;
extern struct cat_command_group at_cmd_system_group;
extern struct cat_command_group at_cmd_evse_group;
extern struct cat_command_group at_cmd_energy_meter_group;
extern struct cat_command_group at_cmd_serial_group;
extern struct cat_command_group at_cmd_network_group;
extern struct cat_command_group at_cmd_board_config_group;

#endif /* AT_H_ */
