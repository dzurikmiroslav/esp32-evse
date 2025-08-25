#include "cat.h"
#include "energy_meter.h"
#include "vars.h"

DEF_AT_VARS_RW_CHECK(vars_energy_meter_mode, CAT_VAR_UINT_DEC, var_u8_1, energy_meter_get_mode, energy_meter_set_mode);

DEF_AT_VARS_RW_CHECK(vars_energy_meter_ac_voltage, CAT_VAR_UINT_DEC, var_u16_1, energy_meter_get_ac_voltage, energy_meter_set_ac_voltage);

DEF_AT_VARS_RW_NO_CHECK(vars_energy_meter_three_phase, CAT_VAR_UINT_DEC, var_u16_1, energy_meter_is_three_phases, energy_meter_set_three_phases);

DEF_AT_VARS_RO(vars_energy_meter_power, CAT_VAR_UINT_DEC, var_u16_1, energy_meter_get_power);

DEF_AT_VARS_RO(vars_energy_meter_session_time, CAT_VAR_UINT_DEC, var_u32_1, energy_meter_get_session_time);

DEF_AT_VARS_RO(vars_energy_meter_charging_time, CAT_VAR_UINT_DEC, var_u32_1, energy_meter_get_charging_time);

DEF_AT_VARS_RO(vars_energy_meter_consumption, CAT_VAR_UINT_DEC, var_u32_1, energy_meter_get_consumption);

static int var_voltage_l1_read(const struct cat_variable* var)
{
    var_i32_1 = energy_meter_get_l1_voltage() * 1000;

    return 0;
}

static int var_voltage_l2_read(const struct cat_variable* var)
{
    var_i32_2 = energy_meter_get_l2_voltage() * 1000;

    return 0;
}

static int var_voltage_l3_read(const struct cat_variable* var)
{
    var_i32_3 = energy_meter_get_l2_voltage() * 1000;

    return 0;
}

static struct cat_variable vars_voltage[] = {
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_1,
        .data_size = sizeof(var_i32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_voltage_l1_read,
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_2,
        .data_size = sizeof(var_i32_2),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_voltage_l2_read,
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_3,
        .data_size = sizeof(var_i32_3),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_voltage_l3_read,
    },
};

static int var_current_l1_read(const struct cat_variable* var)
{
    var_i32_1 = energy_meter_get_l3_current() * 1000;

    return 0;
}

static int var_current_l2_read(const struct cat_variable* var)
{
    var_i32_2 = energy_meter_get_l3_current() * 1000;

    return 0;
}

static int var_current_l3_read(const struct cat_variable* var)
{
    var_i32_3 = energy_meter_get_l3_current() * 1000;

    return 0;
}

static struct cat_variable vars_current[] = {
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_1,
        .data_size = sizeof(var_i32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_current_l1_read,
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_2,
        .data_size = sizeof(var_i32_2),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_current_l2_read,
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_3,
        .data_size = sizeof(var_i32_3),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_current_l3_read,
    },
};

static struct cat_command cmds[] = {
    {
        .name = "+EMETERMODE",
        .var = vars_energy_meter_mode,
        .var_num = sizeof(vars_energy_meter_mode) / sizeof(vars_energy_meter_mode[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERACVOLTAGE",
        .var = vars_energy_meter_ac_voltage,
        .var_num = sizeof(vars_energy_meter_ac_voltage) / sizeof(vars_energy_meter_ac_voltage[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERTHREEPHASE",
        .var = vars_energy_meter_three_phase,
        .var_num = sizeof(vars_energy_meter_three_phase) / sizeof(vars_energy_meter_three_phase[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERPOWER",
        .var = vars_energy_meter_power,
        .var_num = sizeof(vars_energy_meter_power) / sizeof(vars_energy_meter_power[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERSESTIME",
        .var = vars_energy_meter_session_time,
        .var_num = sizeof(vars_energy_meter_session_time) / sizeof(vars_energy_meter_session_time[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERCHTIME",
        .var = vars_energy_meter_charging_time,
        .var_num = sizeof(vars_energy_meter_charging_time) / sizeof(vars_energy_meter_charging_time[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERCONSUM",
        .var = vars_energy_meter_consumption,
        .var_num = sizeof(vars_energy_meter_consumption) / sizeof(vars_energy_meter_consumption[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERVOLTAGE",
        .var = vars_voltage,
        .var_num = sizeof(vars_voltage) / sizeof(vars_voltage[0]),
        .need_all_vars = true,
    },
    {
        .name = "+EMETERCURRENT",
        .var = vars_current,
        .var_num = sizeof(vars_current) / sizeof(vars_current[0]),
        .need_all_vars = true,
    },
};

struct cat_command_group at_cmd_energy_meter_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
