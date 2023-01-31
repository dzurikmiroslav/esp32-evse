#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "cat.h"

#include "at_cmds.h"
#include "wifi.h"
#include "serial.h"
#include "timeout_utils.h"
#include "evse.h"
#include "energy_meter.h"
#include "proximity.h"


bool at_echo = false;

static uint8_t var_u8_1;
static uint8_t var_u8_2;
static uint8_t var_u8_3;
static uint8_t var_u8_4;
static uint16_t var_u16_1;
static int32_t var_i32_1;
static char var_str32_1[32];
static char var_str32_2[32];

static cat_return_state cmd_cmd_run(const struct cat_command* cmd)
{
    return CAT_RETURN_STATE_PRINT_CMD_LIST_OK;
}

static cat_return_state cmd_echo_run(const struct cat_command* cmd)
{
    at_echo = cmd->name[strlen(cmd->name) - 1] == '1';

    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_rst_run(const struct cat_command* cmd)
{
    timeout_restart();

    return CAT_RETURN_STATE_OK;
}

static cat_return_state cmd_wifi_config_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
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

static int var_wifi_config_enabled_read(const struct cat_variable* var)
{
    var_u8_1 = wifi_get_enabled();

    return 0;
}

static int var_wifi_config_ssid_read(const struct cat_variable* var)
{
    wifi_get_ssid(var_str32_1);

    return 0;
}

static struct cat_variable vars_wifi_config[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "enabled",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = var_wifi_config_enabled_read
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .name = "ssid",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = var_wifi_config_ssid_read
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_2,
        .data_size = sizeof(var_str32_2),
        .name = "password",
        .access = CAT_VAR_ACCESS_WRITE_ONLY
    }
};

static int var_wifi_connection_read(const struct cat_variable* var)
{
    var_u8_1 = xEventGroupGetBits(wifi_event_group) & WIFI_STA_CONNECTED_BIT;

    return 0;
}

static struct cat_variable vars_wifi_connection[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "connection",
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_wifi_connection_read
    }
};

static int var_wifi_mode_read(const struct cat_variable* var)
{
    var_u8_1 = 0;
    if (xEventGroupGetBits(wifi_event_group) & WIFI_STA_MODE_BIT) {
        var_u8_1 = 1;
    }
    if (xEventGroupGetBits(wifi_event_group) & WIFI_AP_MODE_BIT) {
        var_u8_1 = 2;
    }

    return 0;
}

static int var_wifi_mode_write(const struct cat_variable* var, const size_t write_size)
{
    if (var_u8_1 == 1) {
        wifi_ap_stop();
    }
    if (var_u8_1 == 2) {
        wifi_ap_start();
    }

    return 0;
}

static struct cat_variable vars_wifi_mode[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "mode",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = var_wifi_mode_read,
        .write = var_wifi_mode_write
    }
};

static int var_serial_config_mode_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    serial_id_t id = cmd->name[strlen(cmd->name) - 1] - '1';
    var_u8_1 = serial_get_mode(id);

    return 0;
}

static int var_serial_config_baud_rate_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    serial_id_t id = cmd->name[strlen(cmd->name) - 1] - '1';
    var_i32_1 = serial_get_baud_rate(id);

    return 0;
}

static int var_serial_config_data_bits_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    serial_id_t id = cmd->name[strlen(cmd->name) - 1] - '1';
    var_u8_2 = 5 + serial_get_data_bits(id);

    return 0;
}

static int var_serial_config_stop_bits_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    serial_id_t id = cmd->name[strlen(cmd->name) - 1] - '1';
    var_u8_3 = 1 + serial_get_stop_bits(id);

    return 0;
}

static int var_serial_config_parity_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    serial_id_t id = cmd->name[strlen(cmd->name) - 1] - '1';
    switch (serial_get_parity(id)) {
    case UART_PARITY_DISABLE:
        var_u8_3 = 0;
        break;
    case UART_PARITY_ODD:
        var_u8_3 = 1;
        break;
    case UART_PARITY_EVEN:
        var_u8_3 = 2;
        break;
    }

    return 0;
}

static cat_return_state cmd_serial_config_write(const struct cat_command* cmd, const uint8_t* data, const size_t data_size, const size_t args_num)
{
    serial_id_t id = cmd->name[strlen(cmd->name) - 1] - '1';
    serial_mode_t mode = var_u8_1;
    int baud_rate = var_i32_1;
    uart_word_length_t data_bits = var_u8_2 - 5;
    uart_stop_bits_t stop_bits = var_u8_3 - 1;
    uart_parity_t parity;
    switch (var_u8_4) {
    case 1:
        parity = UART_PARITY_ODD;
        break;
    case 2:
        parity = UART_PARITY_EVEN;
        break;
    default:
        parity = UART_PARITY_DISABLE;
        break;
    }

    return serial_set_config(id, mode, baud_rate, data_bits, stop_bits, parity) == ESP_OK ? CAT_RETURN_STATE_OK : CAT_RETURN_STATE_ERROR;
}

static struct cat_variable vars_serial_config[] = {
     {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "mode",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_serial_config_mode_read
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_1,
        .data_size = sizeof(var_i32_1),
        .name = "baud_rate",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_serial_config_baud_rate_read
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_2,
        .data_size = sizeof(var_u8_2),
        .name = "data_bits",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_serial_config_data_bits_read
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_3,
        .data_size = sizeof(var_u8_3),
        .name = "stop_bits",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_serial_config_stop_bits_read
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_u8_4,
        .data_size = sizeof(var_u8_4),
        .name = "parity",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_serial_config_parity_read
    }
};

static int var_charging_current_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u16_1 = evse_get_charging_current();

    return 0;
}

static int var_charging_current_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    return evse_set_charging_current(var_u16_1) != ESP_OK;
}

static struct cat_variable vars_charging_current[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u16_1,
        .data_size = sizeof(var_u16_1),
        .name = "current",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_charging_current_read,
        .write_ex = var_charging_current_write
    }
};

static int var_default_charging_current_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u16_1 = evse_get_default_charging_current();

    return 0;
}

static int var_default_charging_current_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    return evse_set_default_charging_current(var_u16_1) != ESP_OK;
}

static struct cat_variable vars_default_charging_current[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u16_1,
        .data_size = sizeof(var_u16_1),
        .name = "current",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_default_charging_current_read,
        .write_ex = var_default_charging_current_write
    }
};

static int var_socket_outlet_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u8_1 = evse_get_socket_outlet();

    return 0;
}

static int var_socket_outlet_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    return  evse_set_socket_outlet(var_u8_1) != ESP_OK;;
}

static struct cat_variable vars_socket_outlet[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "socket_outlet",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_socket_outlet_read,
        .write_ex = var_socket_outlet_write
    }
};

static int var_require_auth_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u8_1 = evse_is_require_auth();

    return 0;
}

static int var_require_auth_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    evse_set_require_auth(var_u8_1);

    return 0;
}

static struct cat_variable vars_require_auth[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "require_auth",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_require_auth_read,
        .write_ex = var_require_auth_write
    }
};

static int var_energy_meter_mode_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u8_1 = energy_meter_get_mode();

    return 0;
}

static int var_energy_meter_mode_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    return energy_meter_set_mode(var_u8_1) != ESP_OK;
}

static struct cat_variable vars_energy_meter_mode[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "mode",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_energy_meter_mode_read,
        .write_ex = var_energy_meter_mode_write
    }
};

static int var_energy_meter_ac_voltage_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u16_1 = energy_meter_get_ac_voltage();

    return 0;
}

static int var_energy_meter_ac_voltage_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    return energy_meter_set_ac_voltage(var_u16_1) != ESP_OK;
}

static struct cat_variable vars_energy_meter_ac_voltage[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u16_1,
        .data_size = sizeof(var_u16_1),
        .name = "voltage",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_energy_meter_ac_voltage_read,
        .write_ex = var_energy_meter_ac_voltage_write
    }
};

static int var_energy_meter_pulse_amount_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u16_1 = energy_meter_get_pulse_amount();

    return 0;
}

static int var_energy_meter_pulse_amount_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    return energy_meter_set_pulse_amount(var_u16_1) != ESP_OK;
}

static struct cat_variable vars_energy_meter_pulse_amount[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u16_1,
        .data_size = sizeof(var_u16_1),
        .name = "pulse_amount",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_energy_meter_pulse_amount_read,
        .write_ex = var_energy_meter_pulse_amount_write
    }
};

static cat_return_state cmd_authorize_run(const struct cat_command* cmd)
{
    evse_authorize();

    return CAT_RETURN_STATE_OK;
}

static int var_enabled_read(const struct cat_variable* var, const struct cat_command* cmd)
{
    var_u8_1 = evse_is_enabled();

    return 0;
}

static int var_enabled_write(const struct cat_variable* var, const struct cat_command* cmd, const size_t write_size)
{
    evse_set_enabled(var_u8_1);

    return 0;
}

static struct cat_variable vars_enabled[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .name = "enabled",
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read_ex = var_enabled_read,
        .write_ex = var_enabled_write
    }
};

static struct cat_command cmds[] = {
    {
        .name = "+CMD",
        .run = cmd_cmd_run
    },
    {
        .name = "E0",
        .run = cmd_echo_run
    },
    {
        .name = "E1",
        .run = cmd_echo_run
    },
    {
        .name = "+RST",
        .run = cmd_rst_run
    },
    {
        .name = "+WIFICFG",
        .write = cmd_wifi_config_write,
        .var = vars_wifi_config,
        .var_num = sizeof(vars_wifi_config) / sizeof(vars_wifi_config[0])
    },
    {
        .name = "+WIFICON",
        .var = vars_wifi_connection,
        .var_num = sizeof(vars_wifi_connection) / sizeof(vars_wifi_connection[0])
    },
    {
        .name = "+WIFIMODE",
        .var = vars_wifi_mode,
        .var_num = sizeof(vars_wifi_mode) / sizeof(vars_wifi_mode[0]),
        .need_all_vars = true
    },
    {
        .name = "+SRLCFG1",
        .write = cmd_serial_config_write,
        .var = vars_serial_config,
        .var_num = sizeof(vars_serial_config) / sizeof(vars_serial_config[0]),
        .need_all_vars = true
    },
    {
        .name = "+SRLCFG2",
        .write = cmd_serial_config_write,
        .var = vars_serial_config,
        .var_num = sizeof(vars_serial_config) / sizeof(vars_serial_config[0]),
        .need_all_vars = true
    },
#if SOC_UART_NUM > 2
    {
        .name = "+SRLCFG3",
        .write = cmd_serial_config_write,
        .var = vars_serial_config,
        .var_num = sizeof(vars_serial_config) / sizeof(vars_serial_config[0]),
        .need_all_vars = true
    },
#endif
    {
        .name = "+CHCUR",
        .var = vars_charging_current,
        .var_num = sizeof(vars_charging_current) / sizeof(vars_charging_current[0]),
        .need_all_vars = true
    },
    {
        .name = "+DCHCUR",
        .var = vars_default_charging_current,
        .var_num = sizeof(vars_default_charging_current) / sizeof(vars_default_charging_current[0]),
        .need_all_vars = true
    },
    {
        .name = "+SCOU",
        .var = vars_socket_outlet,
        .var_num = sizeof(vars_socket_outlet) / sizeof(vars_socket_outlet[0]),
        .need_all_vars = true
    },
    {
        .name = "+RQAUTH",
        .var = vars_require_auth,
        .var_num = sizeof(vars_require_auth) / sizeof(vars_require_auth[0]),
        .need_all_vars = true
    },
    {
        .name = "+EMMODE",
        .var = vars_energy_meter_mode,
        .var_num = sizeof(vars_energy_meter_mode) / sizeof(vars_energy_meter_mode[0]),
        .need_all_vars = true
    },
    {
        .name = "+EMACVLT",
        .var = vars_energy_meter_ac_voltage,
        .var_num = sizeof(vars_energy_meter_ac_voltage) / sizeof(vars_energy_meter_ac_voltage[0]),
        .need_all_vars = true
    },
    {
        .name = "+EMPLAM",
        .var = vars_energy_meter_pulse_amount,
        .var_num = sizeof(vars_energy_meter_pulse_amount) / sizeof(vars_energy_meter_pulse_amount[0]),
        .need_all_vars = true
    },
    {
        .name = "+AUTH",
        .run = cmd_authorize_run
    },
    {
        .name = "+EN",
        .var = vars_enabled,
        .var_num = sizeof(vars_enabled) / sizeof(vars_enabled[0]),
        .need_all_vars = true
    },
};

// static uint8_t buf[BUF_SIZE];

struct cat_command_group at_cmd_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};

// static struct cat_descriptor desc = {
//     .cmd_group = cmd_desc,
//     .cmd_group_num = sizeof(cmd_desc) / sizeof(cmd_desc[0]),
//     .buf = buf,
//     .buf_size = sizeof(buf)
// };

// static int write_char(char ch)
// {
//     if (ch == 10) {
//         uart_write_bytes(uart_num, "\r\n", 2);
//     } else {
//         uart_write_bytes(uart_num, &ch, 1);
//     }

//     return 1;
// }

// static int read_char(char* ch)
// {
//     int readed = uart_read_bytes(uart_num, ch, 1, 1);

//     if (readed > 0) {
//         if (*ch == 13) {
//             *ch = 10;
//         }
//         if (echo_flag) {
//             write_char(*ch);
//         }
//     }

//     return readed;
// }

// static struct cat_io_interface iface = {
//     .read = read_char,
//     .write = write_char
// };

// void at_init(void)
// {
//     cat_init(&at, &desc, &iface, NULL);
// }

// void at_process(int port)
// {
//     uart_num = port;

//     while (cat_service(&at) != 0) {};
// }