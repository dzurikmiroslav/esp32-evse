#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lua.h"
#include "lauxlib.h"

#include "lua_evse.h"
#include "evse.h"
#include "energy_meter.h"
#include "script_utils.h"
#include "temp_sensor.h"

#include "esp_log.h"

static int l_state(lua_State* L)
{
    lua_pushinteger(L, evse_get_state());
    return 1;
}

static int l_error(lua_State* L)
{
    lua_pushinteger(L, evse_get_error());
    return 1;
}

static int l_enabled(lua_State* L)
{
    lua_pushboolean(L, evse_is_enabled());
    return 1;
}


// static int l_set_enabled(lua_State* L)
// {
//     int top = be_top(vm);
//     if (top == 2 && be_isbool(vm, 2)) {
//         evse_set_enabled(be_tobool(vm, 2));
//     } else {
//         be_raise(vm, "type_error", NULL);
//     }
//     be_return(vm);
// }

// static int l_available(lua_State* L)
// {
//     be_pushbool(vm, evse_is_available());
//     be_return(vm);
// }

// static int l_set_available(lua_State* L)
// {
//     int top = be_top(vm);
//     if (top == 2 && be_isbool(vm, 2)) {
//         evse_set_available(be_tobool(vm, 2));
//     } else {
//         be_raise(vm, "type_error", NULL);
//     }
//     be_return(vm);
// }

// static int l_charging_current(lua_State* L)
// {
//     be_pushreal(vm, evse_get_charging_current() / 10.0f);
//     be_return(vm);
// }

// static int l_set_charging_current(lua_State* L)
// {
//     int top = be_top(vm);
//     if (top == 2 && be_isnumber(vm, 2)) {
//         uint16_t value = round(be_toreal(vm, 2) * 10);
//         if (evse_set_charging_current(value) != ESP_OK) {
//             be_raise(vm, "value_error", "invalid value");
//         }
//     } else {
//         be_raise(vm, "type_error", NULL);
//     }
//     be_return(vm);
// }

// static int l_power(lua_State* L)
// {
//     be_pushint(vm, energy_meter_get_power());
//     be_return(vm);
// }

// static int l_charging_time(lua_State* L)
// {
//     be_pushint(vm, energy_meter_get_charging_time());
//     be_return(vm);
// }

// static int l_session_time(lua_State* L)
// {
//     be_pushint(vm, energy_meter_get_session_time());
//     be_return(vm);
// }

// static int l_consumption(lua_State* L)
// {
//     be_pushint(vm, energy_meter_get_consumption());
//     be_return(vm);
// }

// static int l_voltage(lua_State* L)
// {
//     be_newobject(vm, "list");

//     be_pushreal(vm, energy_meter_get_l1_voltage());
//     be_data_push(vm, -2);
//     be_pop(vm, 1);

//     be_pushreal(vm, energy_meter_get_l2_voltage());
//     be_data_push(vm, -2);
//     be_pop(vm, 1);

//     be_pushreal(vm, energy_meter_get_l3_voltage());
//     be_data_push(vm, -2);
//     be_pop(vm, 1);

//     be_pop(vm, 1);
//     be_return(vm);
// }

// static int l_current(lua_State* L)
// {
//     be_newobject(vm, "list");

//     be_pushreal(vm, energy_meter_get_l1_current());
//     be_data_push(vm, -2);
//     be_pop(vm, 1);

//     be_pushreal(vm, energy_meter_get_l2_current());
//     be_data_push(vm, -2);
//     be_pop(vm, 1);

//     be_pushreal(vm, energy_meter_get_l3_current());
//     be_data_push(vm, -2);
//     be_pop(vm, 1);

//     be_pop(vm, 1);
//     be_return(vm);
// }

// static int l_low_temperature(lua_State* L)
// {
//     be_pushreal(vm, temp_sensor_get_low() / 100);
//     be_return(vm);
// }

// static int l_high_temperature(lua_State* L)
// {
//     be_pushreal(vm, temp_sensor_get_high() / 100);
//     be_return(vm);
// }

static const luaL_Reg lib[] = {
    {"STATE_A",     NULL},
    {"STATE_B1",    NULL},
    {"STATE_B2",    NULL},
    {"STATE_C1",    NULL},
    {"STATE_C2",    NULL},
    {"STATE_D1",    NULL},
    {"STATE_D2",    NULL},
    {"STATE_E",     NULL},
    {"STATE_F",     NULL},
    {"ERR_PILOT_FAULT_BIT",             NULL},
    {"ERR_DIODE_SHORT_BIT",             NULL},
    {"ERR_LOCK_FAULT_BIT",              NULL},
    {"ERR_UNLOCK_FAULT_BIT",            NULL},
    {"ERR_RCM_TRIGGERED_BIT",           NULL},
    {"ERR_RCM_SELFTEST_FAULT_BIT",      NULL},
    {"ERR_TEMPERATURE_HIGH_BIT",        NULL},
    {"ERR_TEMPERATURE_FAULT_BIT",       NULL},
    {"state",       l_state},
    {"error",       l_error},
    {"enabled",     l_enabled},
    {NULL, NULL}
};

int lua_open_evse(lua_State* L)
{
    luaL_newlib(L, lib);

    lua_pushinteger(L, EVSE_STATE_A);
    lua_setfield(L, -2, "STATE_A");

    lua_pushinteger(L, EVSE_STATE_B1);
    lua_setfield(L, -2, "STATE_B1");

    lua_pushinteger(L, EVSE_STATE_B2);
    lua_setfield(L, -2, "STATE_B2");

    lua_pushinteger(L, EVSE_STATE_C1);
    lua_setfield(L, -2, "STATE_C1");

    lua_pushinteger(L, EVSE_STATE_C2);
    lua_setfield(L, -2, "STATE_C2");

    lua_pushinteger(L, EVSE_STATE_D1);
    lua_setfield(L, -2, "STATE_D1");

    lua_pushinteger(L, EVSE_STATE_D2);
    lua_setfield(L, -2, "STATE_D2");

    lua_pushinteger(L, EVSE_STATE_E);
    lua_setfield(L, -2, "STATE_E");

    lua_pushinteger(L, EVSE_STATE_F);
    lua_setfield(L, -2, "STATE_F");

    lua_pushinteger(L, EVSE_ERR_PILOT_FAULT_BIT);
    lua_setfield(L, -2, "ERR_PILOT_FAULT_BIT");

    lua_pushinteger(L, EVSE_ERR_DIODE_SHORT_BIT);
    lua_setfield(L, -2, "ERR_DIODE_SHORT_BIT");

    lua_pushinteger(L, EVSE_ERR_LOCK_FAULT_BIT);
    lua_setfield(L, -2, "ERR_LOCK_FAULT_BIT");

    lua_pushinteger(L, EVSE_ERR_UNLOCK_FAULT_BIT);
    lua_setfield(L, -2, "ERR_UNLOCK_FAULT_BIT");

    lua_pushinteger(L, EVSE_ERR_RCM_TRIGGERED_BIT);
    lua_setfield(L, -2, "ERR_RCM_TRIGGERED_BIT");

    lua_pushinteger(L, EVSE_ERR_RCM_SELFTEST_FAULT_BIT);
    lua_setfield(L, -2, "ERR_RCM_SELFTEST_FAULT_BIT");

    lua_pushinteger(L, EVSE_ERR_TEMPERATURE_HIGH_BIT);
    lua_setfield(L, -2, "ERR_TEMPERATURE_HIGH_BIT");
    
    lua_pushinteger(L, EVSE_ERR_TEMPERATURE_FAULT_BIT);
    lua_setfield(L, -2, "ERR_TEMPERATURE_FAULT_BIT");

    return 1;
}
