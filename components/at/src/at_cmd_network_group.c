#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "at.h"
#include "cat.h"
#include "discovery.h"
#include "vars.h"
#include "wifi.h"

static int vars_sta_config_read(const struct cat_variable* var)
{
    var_u8_1 = wifi_is_enabled();
    wifi_get_ssid(var_str32_1);

    return 0;
}

static struct cat_variable vars_sta_config[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = vars_sta_config_read,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_2,
        .data_size = sizeof(var_str32_2),
        .access = CAT_VAR_ACCESS_WRITE_ONLY,
    },
};

static cat_return_state vars_sta_config_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
{
    bool enabled = var_u8_1 > 0;
    const char* ssid = NULL;
    const char* password = NULL;

    if (args_num > 1) {
        ssid = var_str32_1;
    }
    if (args_num > 2) {
        password = var_str32_2;
    }

    return wifi_set_config(enabled, ssid, password) == ESP_OK ? CAT_RETURN_STATE_OK : CAT_RETURN_STATE_ERROR;
}

static int vars_sta_static_config_read(const struct cat_variable* var)
{
    var_u8_1 = wifi_is_static_enabled();
    wifi_get_static_ip(var_str32_1);
    wifi_get_static_gateway(var_str32_2);
    wifi_get_static_netmask(var_str32_3);

    return 0;
}

static struct cat_variable vars_sta_static_config[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = vars_sta_static_config_read,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_2,
        .data_size = sizeof(var_str32_2),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_3,
        .data_size = sizeof(var_str32_3),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
};

static cat_return_state vars_sta_static_config_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
{
    bool enabled = var_u8_1 > 0;
    const char* ip = NULL;
    const char* gateway = NULL;
    const char* netmask = NULL;

    if (args_num > 1) {
        ip = var_str32_1;
    }
    if (args_num > 2) {
        gateway = var_str32_2;
    }
    if (args_num > 3) {
        netmask = var_str32_3;
    }

    return wifi_set_static_config(enabled, ip, gateway, netmask) == ESP_OK ? CAT_RETURN_STATE_OK : CAT_RETURN_STATE_ERROR;
}

static int vars_ap_config_read(const struct cat_variable* var)
{
    var_u8_1 = (xEventGroupGetBits(wifi_event_group) & WIFI_AP_MODE_BIT) > 0 ? 1 : 0;
    wifi_get_ap_ssid(var_str32_1);

    return 0;
}

static cat_return_state vars_ap_config_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
{
    if (var_u8_1) {
        wifi_ap_start();
    } else {
        wifi_ap_stop();
    }

    return 0;
}

static struct cat_variable vars_ap_config[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = vars_ap_config_read,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static int vars_sta_connection_read(const struct cat_variable* var)
{
    var_u8_1 = (xEventGroupGetBits(wifi_event_group) & WIFI_STA_CONNECTED_BIT) > 0 ? 1 : 0;
    var_i8_1 = wifi_get_rssi();

    return 0;
}

static struct cat_variable vars_sta_connection[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = vars_sta_connection_read,
    },
    { .type = CAT_VAR_INT_DEC, .data = &var_i8_1, .data_size = sizeof(var_i8_1), .access = CAT_VAR_ACCESS_READ_ONLY },
};

static int var_ap_connection_connected_read(const struct cat_variable* var)
{
    var_u8_1 = (xEventGroupGetBits(wifi_event_group) & WIFI_AP_CONNECTED_BIT) > 0 ? 1 : 0;

    return 0;
}

static struct cat_variable vars_ap_connection[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_ap_connection_connected_read,
    },
};

static int var_sta_ip_read(const struct cat_variable* var)
{
    wifi_get_ip(false, var_str32_1);

    return 0;
}

static struct cat_variable vars_sta_ip[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_sta_ip_read,
    },
};

static int var_sta_mac_read(const struct cat_variable* var)
{
    wifi_get_mac(false, var_str32_1);

    return 0;
}

static struct cat_variable vars_sta_mac[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_sta_mac_read,
    },
};

static int var_ap_ip_read(const struct cat_variable* var)
{
    wifi_get_ip(true, var_str32_1);

    return 0;
}

static struct cat_variable vars_ap_ip[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_ap_ip_read,
    },
};

static int var_ap_mac_read(const struct cat_variable* var)
{
    wifi_get_mac(true, var_str32_1);

    return 0;
}

static struct cat_variable vars_ap_mac[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_ap_mac_read,
    },
};

static struct cat_variable vars_scan_ap[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i8_1,
        .data_size = sizeof(var_i8_1),
    },
};

static void pop_wifi_ap_scan_result(at_task_context_t* context)
{
    if (!SLIST_EMPTY(context->wifi_scan_ap_list)) {
        wifi_scan_ap_entry_t* item = SLIST_FIRST(context->wifi_scan_ap_list);

        strcpy(var_str32_1, item->ssid);
        var_u8_1 = item->auth;
        var_i8_1 = item->rssi;

        SLIST_REMOVE_HEAD(context->wifi_scan_ap_list, entries);

        wifi_scan_aps_entry_free(item);
    } else {
        free((void*)context->wifi_scan_ap_list);
        context->wifi_scan_ap_list = NULL;
    }
}

static cat_return_state cmd_scan_ap_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size);

// unsolicited command
static struct cat_command cmd_scan_ap = {
    .name = "+WIFIAPSCAN",
    .read = cmd_scan_ap_read,
    .var = vars_scan_ap,
    .var_num = sizeof(vars_scan_ap) / sizeof(vars_scan_ap[0]),
};

// unsolicited read callback handler
static cat_return_state cmd_scan_ap_read(const struct cat_command* cmd, uint8_t* data, size_t* data_size, const size_t max_data_size)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    if (context->wifi_scan_ap_list) {
        pop_wifi_ap_scan_result(context);
        cat_trigger_unsolicited_read(context->at, &cmd_scan_ap);

        return CAT_RETURN_STATE_DATA_NEXT;
    } else {
        return CAT_RETURN_STATE_HOLD_EXIT_OK;
    }
}

static cat_return_state cmd_ap_scan_run(const struct cat_command* cmd)
{
    at_task_context_t* context = (at_task_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, AT_TASK_CONTEXT_INDEX);

    context->wifi_scan_ap_list = wifi_scan_aps();

    pop_wifi_ap_scan_result(context);
    cat_trigger_unsolicited_read(context->at, &cmd_scan_ap);

    return CAT_RETURN_STATE_HOLD;
}

static int var_hostname_read(const struct cat_variable* var)
{
    discovery_get_hostname(var_str32_1);

    return 0;
}

static int var_hostname_write(const struct cat_variable* var, const size_t write_size)
{
    return discovery_set_hostname(var_str32_1) != ESP_OK;
}

static struct cat_variable vars_hostname[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = var_hostname_read,
        .write = var_hostname_write,
    },
};

static int var_instance_name_read(const struct cat_variable* var)
{
    discovery_get_instance_name(var_str32_1);

    return 0;
}

static int var_instance_name_write(const struct cat_variable* var, const size_t write_size)
{
    return discovery_set_instance_name(var_str32_1) != ESP_OK;
}

static struct cat_variable vars_instance_name[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = var_instance_name_read,
        .write = var_instance_name_write,
    },
};

static struct cat_command cmds[] = {
    {
        .name = "+WIFISTACFG",
        .var = vars_sta_config,
        .var_num = sizeof(vars_sta_config) / sizeof(vars_sta_config[0]),
        .write = vars_sta_config_write,
    },
    {
        .name = "+WIFIAPCFG",
        .var = vars_ap_config,
        .var_num = sizeof(vars_ap_config) / sizeof(vars_ap_config[0]),
        .write = vars_ap_config_write,
    },
    {
        .name = "+WIFISTACONN",
        .var = vars_sta_connection,
        .var_num = sizeof(vars_sta_connection) / sizeof(vars_sta_connection[0]),
    },
    {
        .name = "+WIFIAPCONN",
        .var = vars_ap_connection,
        .var_num = sizeof(vars_ap_connection) / sizeof(vars_ap_connection[0]),
    },
    {
        .name = "+WIFISTASTATIC",
        .var = vars_sta_static_config,
        .var_num = sizeof(vars_sta_static_config) / sizeof(vars_sta_static_config[0]),
        .write = vars_sta_static_config_write,
    },
    {
        .name = "+WIFISTAIP",
        .var = vars_sta_ip,
        .var_num = sizeof(vars_sta_ip) / sizeof(vars_sta_ip[0]),
    },
    {
        .name = "+WIFISTAMAC",
        .var = vars_sta_mac,
        .var_num = sizeof(vars_sta_mac) / sizeof(vars_sta_mac[0]),
    },
    {
        .name = "+WIFIAPIP",
        .var = vars_ap_ip,
        .var_num = sizeof(vars_ap_ip) / sizeof(vars_ap_ip[0]),
    },
    {
        .name = "+WIFIAPMAC",
        .var = vars_ap_mac,
        .var_num = sizeof(vars_ap_mac) / sizeof(vars_ap_mac[0]),
    },
    {
        .name = "+WIFIAPSCAN",
        .run = cmd_ap_scan_run,
    },
    {
        .name = "+HOSTNAME",
        .var = vars_hostname,
        .var_num = sizeof(vars_hostname) / sizeof(vars_hostname[0]),
    },
    {
        .name = "+INSTNAME",
        .var = vars_instance_name,
        .var_num = sizeof(vars_instance_name) / sizeof(vars_instance_name[0]),
    },
};

struct cat_command_group at_cmd_network_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
