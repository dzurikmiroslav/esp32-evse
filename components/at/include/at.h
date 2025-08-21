#ifndef AT_H_
#define AT_H_

#include <stdbool.h>
#include <stdint.h>

#include "cat.h"
#include "wifi.h"

#define AT_CMD_GROUPS                                                                                                                                                   \
    {                                                                                                                                                                   \
        &at_cmd_basic_group, &at_cmd_system_group, &at_cmd_evse_group, &at_cmd_energy_meter_group, &at_cmd_wifi_group, &at_cmd_serial_group, &at_cmd_board_config_group \
    }

#define AT_TASK_CONTEXT_INDEX 1

#define AT_TASK_CONTEXT(at_ptr)                                \
    {                                                          \
        .at = at_ptr, .echo = true, .wifi_scan_ap_list = NULL, \
    }

typedef struct {
    struct cat_object* at;
    bool echo;
    wifi_scan_ap_list_t* wifi_scan_ap_list;
    uint8_t serial_index;
    uint8_t aux_input_index;
    uint8_t aux_output_index;
    uint8_t aux_analog_input_index;
} at_task_context_t;

void at_task_context_clean(at_task_context_t* context);

extern struct cat_command_group at_cmd_basic_group;
extern struct cat_command_group at_cmd_system_group;
extern struct cat_command_group at_cmd_evse_group;
extern struct cat_command_group at_cmd_energy_meter_group;
extern struct cat_command_group at_cmd_serial_group;
extern struct cat_command_group at_cmd_wifi_group;
extern struct cat_command_group at_cmd_board_config_group;

#endif /* AT_H_ */