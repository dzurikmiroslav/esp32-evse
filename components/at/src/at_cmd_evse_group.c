#include "cat.h"
#include "evse.h"
#include "vars.h"

DEF_AT_VARS_RO(vars_state, CAT_VAR_UINT_DEC, var_u8_1, evse_get_state);

DEF_AT_VARS_RO(vars_error, CAT_VAR_UINT_DEC, var_u32_1, evse_get_error);

DEF_AT_VARS_RW_NO_CHECK(vars_enabled, CAT_VAR_UINT_DEC, var_u8_1, evse_is_enabled, evse_set_enabled);

DEF_AT_VARS_RW_NO_CHECK(vars_available, CAT_VAR_UINT_DEC, var_u8_1, evse_is_available, evse_set_available);

DEF_AT_VARS_RW_NO_CHECK(vars_require_auth, CAT_VAR_UINT_DEC, var_u8_1, evse_is_require_auth, evse_set_require_auth);

static cat_return_state cmd_authorize_run(const struct cat_command* cmd)
{
    evse_authorize();

    return CAT_RETURN_STATE_OK;
}

DEF_AT_VARS_RO(vars_pending_auth, CAT_VAR_UINT_DEC, var_u8_1, evse_is_pending_auth);

DEF_AT_VARS_RW_CHECK(vars_charging_current, CAT_VAR_UINT_DEC, var_u16_1, evse_get_charging_current, evse_set_charging_current);

DEF_AT_VARS_RW_CHECK(vars_default_charging_current, CAT_VAR_UINT_DEC, var_u16_1, evse_get_default_charging_current, evse_set_default_charging_current);

DEF_AT_VARS_RW_CHECK(vars_max_charging_current, CAT_VAR_UINT_DEC, var_u8_1, evse_get_max_charging_current, evse_set_max_charging_current);

DEF_AT_VARS_RW_NO_CHECK(vars_consumption_limit, CAT_VAR_UINT_DEC, var_u32_1, evse_get_consumption_limit, evse_set_consumption_limit);

DEF_AT_VARS_RW_NO_CHECK(vars_charging_time_limit, CAT_VAR_UINT_DEC, var_u32_1, evse_get_charging_time_limit, evse_set_charging_time_limit);

DEF_AT_VARS_RW_NO_CHECK(vars_default_charging_time_limit, CAT_VAR_UINT_DEC, var_u32_1, evse_get_default_charging_time_limit, evse_set_default_charging_time_limit);

DEF_AT_VARS_RW_NO_CHECK(vars_under_power_limit, CAT_VAR_UINT_DEC, var_u32_1, evse_get_under_power_limit, evse_set_under_power_limit);

DEF_AT_VARS_RW_NO_CHECK(vars_default_under_power_limit, CAT_VAR_UINT_DEC, var_u32_1, evse_get_default_under_power_limit, evse_set_default_under_power_limit);

DEF_AT_VARS_RO(vars_limit_reached, CAT_VAR_UINT_DEC, var_u8_1, evse_is_limit_reached);

DEF_AT_VARS_RW_NO_CHECK(vars_default_consumption_limit, CAT_VAR_UINT_DEC, var_u32_1, evse_get_default_consumption_limit, evse_set_default_consumption_limit);

DEF_AT_VARS_RW_CHECK(vars_socket_outlet, CAT_VAR_UINT_DEC, var_u8_1, evse_get_socket_outlet, evse_set_socket_outlet);

static struct cat_command cmds[] = {
    {
        .name = "+STATE",
        .var = vars_state,
        .var_num = sizeof(vars_state) / sizeof(vars_state[0]),
    },
    {
        .name = "+ERROR",
        .var = vars_error,
        .var_num = sizeof(vars_error) / sizeof(vars_error[0]),
    },
    {
        .name = "+ENABLE",
        .var = vars_enabled,
        .var_num = sizeof(vars_enabled) / sizeof(vars_enabled[0]),
        .need_all_vars = true,
    },
    {
        .name = "+AVAILABLE",
        .var = vars_available,
        .var_num = sizeof(vars_available) / sizeof(vars_available[0]),
        .need_all_vars = true,
    },
    {
        .name = "+REQAUTH",
        .var = vars_require_auth,
        .var_num = sizeof(vars_require_auth) / sizeof(vars_require_auth[0]),
        .need_all_vars = true,
    },
    {
        .name = "+PENDAUTH",
        .var = vars_pending_auth,
        .var_num = sizeof(vars_pending_auth) / sizeof(vars_pending_auth[0]),
    },
    {
        .name = "+AUTH",
        .run = cmd_authorize_run,
    },
    {
        .name = "+CHCUR",
        .var = vars_charging_current,
        .var_num = sizeof(vars_charging_current) / sizeof(vars_charging_current[0]),
        .need_all_vars = true,
    },
    {
        .name = "+DEFCHCUR",
        .var = vars_default_charging_current,
        .var_num = sizeof(vars_default_charging_current) / sizeof(vars_default_charging_current[0]),
        .need_all_vars = true,
    },
    {
        .name = "+MAXCHCUR",
        .var = vars_max_charging_current,
        .var_num = sizeof(vars_max_charging_current) / sizeof(vars_max_charging_current[0]),
        .need_all_vars = true,
    },
    {
        .name = "+CONSUMLIM",
        .var = vars_consumption_limit,
        .var_num = sizeof(vars_consumption_limit) / sizeof(vars_consumption_limit[0]),
        .need_all_vars = true,
    },
    {
        .name = "+DEFCONSUMLIM",
        .var = vars_default_consumption_limit,
        .var_num = sizeof(vars_default_consumption_limit) / sizeof(vars_default_consumption_limit[0]),
        .need_all_vars = true,
    },
    {
        .name = "+CHTIMELIM",
        .var = vars_charging_time_limit,
        .var_num = sizeof(vars_charging_time_limit) / sizeof(vars_charging_time_limit[0]),
        .need_all_vars = true,
    },
    {
        .name = "+DEFCHTIMELIM",
        .var = vars_default_charging_time_limit,
        .var_num = sizeof(vars_default_charging_time_limit) / sizeof(vars_default_charging_time_limit[0]),
        .need_all_vars = true,
    },
    {
        .name = "+UNDERPOWERLIM",
        .var = vars_under_power_limit,
        .var_num = sizeof(vars_under_power_limit) / sizeof(vars_under_power_limit[0]),
        .need_all_vars = true,
    },
    {
        .name = "+DEFUNDERPOWERLIM",
        .var = vars_default_under_power_limit,
        .var_num = sizeof(vars_default_under_power_limit) / sizeof(vars_default_under_power_limit[0]),
        .need_all_vars = true,
    },
    {
        .name = "+LIMREACH",
        .var = vars_limit_reached,
        .var_num = sizeof(vars_limit_reached) / sizeof(vars_limit_reached[0]),
    },
    {
        .name = "+SOCKETOUTLET",
        .var = vars_socket_outlet,
        .var_num = sizeof(vars_socket_outlet) / sizeof(vars_socket_outlet[0]),
        .need_all_vars = true,
    },
};

struct cat_command_group at_cmd_evse_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
