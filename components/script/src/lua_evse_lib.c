#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lua.h"
#include "lauxlib.h"

#include "lua_evse_lib.h"
#include "evse.h"
#include "energy_meter.h"
#include "script_utils.h"
#include "temp_sensor.h"

#include "esp_log.h"

static int l_get_state(lua_State* L)
{
    lua_pushinteger(L, evse_get_state());
    return 1;
}

static int l_get_error(lua_State* L)
{
    lua_pushinteger(L, evse_get_error());
    return 1;
}

static int l_get_enabled(lua_State* L)
{
    lua_pushboolean(L, evse_is_enabled());
    return 1;
}

static int l_set_enabled(lua_State* L)
{
    luaL_argcheck(L, lua_isboolean(L, 1), 1, "Must be boolean");
    evse_set_enabled(lua_toboolean(L, 1));
    return 0;
}

static int l_get_available(lua_State* L)
{
    lua_pushboolean(L, evse_is_available());
    return 1;
}

static int l_set_available(lua_State* L)
{
    luaL_argcheck(L, lua_isboolean(L, 1), 1, "Must be boolean");
    evse_set_available(lua_toboolean(L, 1));
    return 0;
}

static int l_get_charging_current(lua_State* L)
{
    lua_pushnumber(L, evse_get_charging_current() / 10.0f);
    return 1;
}

static int l_set_charging_current(lua_State* L)
{
    luaL_argcheck(L, lua_isnumber(L, 1), 1, "Must be number");
    uint16_t value = round(lua_tonumber(L, 1) * 10);
    if (evse_set_charging_current(value) != ESP_OK) {
        luaL_argerror(L, 1, "Invalid value");
    }
    return 0;
}

static int l_get_power(lua_State* L)
{
    lua_pushinteger(L, energy_meter_get_power());
    return 1;
}

static int l_get_charging_time(lua_State* L)
{
    lua_pushinteger(L, energy_meter_get_charging_time());
    return 1;
}

static int l_get_session_time(lua_State* L)
{
    lua_pushinteger(L, energy_meter_get_session_time());
    return 1;
}

static int l_get_consumption(lua_State* L)
{
    lua_pushinteger(L, energy_meter_get_consumption());
    return 1;
}

static int l_get_voltage(lua_State* L)
{
    lua_pushnumber(L, energy_meter_get_l1_voltage());
    lua_pushnumber(L, energy_meter_get_l2_voltage());
    lua_pushnumber(L, energy_meter_get_l3_voltage());
    return 3;
}

static int l_get_current(lua_State* L)
{
    lua_pushnumber(L, energy_meter_get_l1_current());
    lua_pushnumber(L, energy_meter_get_l2_current());
    lua_pushnumber(L, energy_meter_get_l3_current());
    return 3;
}

static int l_get_low_temperature(lua_State* L)
{
    lua_pushnumber(L, temp_sensor_get_low() / 100);
    return 1;
}

static int l_get_high_temperature(lua_State* L)
{
    lua_pushnumber(L, temp_sensor_get_high() / 100);
    return 1;
}

static int l_add_driver(lua_State* L)
{
    luaL_argcheck(L, lua_isnumber(L, 1), 1, "Must be table");

    ESP_LOGI("YOLO", "top %d", lua_gettop(L));

    lua_getglobal(L, "evse");
    lua_getfield(L, -1, "__drivers");
    int len = lua_rawlen(L, -1);

  //  lua_insert(L, -3);
     lua_pushstring(L, "aaa");
    lua_rawseti(L, -2, len + 1);

    // ESP_LOGI("YOLO", "top %d", lua_gettop(L));

    // ESP_LOGI("YOLO", "val %d", (int)lua_tointeger(L, -1));

//    lua_settable()


    return 0;
}

static const luaL_Reg lib[] = {
    //states
    {"STATEA",  NULL},
    {"STATEB1", NULL},
    {"STATEB2", NULL},
    {"STATEC1", NULL},
    {"STATEC2", NULL},
    {"STATED1", NULL},
    {"STATED2", NULL},
    {"STATEE",  NULL},
    {"STATEF",  NULL},
    // error bits
    {"ERRPILOTFAULTBIT",        NULL},
    {"ERRDIODESHORTBIT",        NULL},
    {"ERRLOCKFAULTBIT",         NULL},
    {"ERRUNLOCKFAULTBIT",       NULL},
    {"ERRRCMTRIGGEREDBIT",      NULL},
    {"ERRRCMSELFTESTFAULTBIT",  NULL},
    {"ERRTEMPERATUREHIGHBIT",   NULL},
    {"ERRTEMPERATUREFAULTBIT",  NULL},
    // methods
    {"getstate",            l_get_state},
    {"geterror",            l_get_error},
    {"getenabled",          l_get_enabled},
    {"setenabled",          l_set_enabled},
    {"getavailable",        l_get_available},
    {"setavailable",        l_set_available},
    {"getchargingcurrent",  l_get_charging_current},
    {"setchargingcurrent",  l_set_charging_current},
    {"getpower",            l_get_power},
    {"getchargingtime",     l_get_charging_time},
    {"getsessiontime",      l_get_session_time},
    {"getconsumption",      l_get_consumption},
    {"getvoltage",          l_get_voltage},
    {"getcurrent",          l_get_current},
    {"getlowtemperature",   l_get_low_temperature},
    {"gethightemperature",  l_get_high_temperature},
    {"adddriver",           l_add_driver},
    // private fields
    {"__drivers",           NULL},
    {NULL, NULL}
};

int lua_open_evse(lua_State* L)
{
    luaL_newlib(L, lib);

    lua_pushinteger(L, EVSE_STATE_A);
    lua_setfield(L, -2, "STATEA");

    lua_pushinteger(L, EVSE_STATE_B1);
    lua_setfield(L, -2, "STATEB1");

    lua_pushinteger(L, EVSE_STATE_B2);
    lua_setfield(L, -2, "STATEB2");

    lua_pushinteger(L, EVSE_STATE_C1);
    lua_setfield(L, -2, "STATEC1");

    lua_pushinteger(L, EVSE_STATE_C2);
    lua_setfield(L, -2, "STATEC2");

    lua_pushinteger(L, EVSE_STATE_D1);
    lua_setfield(L, -2, "STATED1");

    lua_pushinteger(L, EVSE_STATE_D2);
    lua_setfield(L, -2, "STATED2");

    lua_pushinteger(L, EVSE_STATE_E);
    lua_setfield(L, -2, "STATEE");

    lua_pushinteger(L, EVSE_STATE_F);
    lua_setfield(L, -2, "STATEF");

    lua_pushinteger(L, EVSE_ERR_PILOT_FAULT_BIT);
    lua_setfield(L, -2, "ERRPILOTFAULTBIT");

    lua_pushinteger(L, EVSE_ERR_DIODE_SHORT_BIT);
    lua_setfield(L, -2, "ERRDIODESHORTBIT");

    lua_pushinteger(L, EVSE_ERR_LOCK_FAULT_BIT);
    lua_setfield(L, -2, "ERRLOCKFAULTBIT");

    lua_pushinteger(L, EVSE_ERR_UNLOCK_FAULT_BIT);
    lua_setfield(L, -2, "ERRUNLOCKFAULTBIT");

    lua_pushinteger(L, EVSE_ERR_RCM_TRIGGERED_BIT);
    lua_setfield(L, -2, "ERRRCMTRIGGEREDBIT");

    lua_pushinteger(L, EVSE_ERR_RCM_SELFTEST_FAULT_BIT);
    lua_setfield(L, -2, "ERRRCMSELFTESTFAULTBIT");

    lua_pushinteger(L, EVSE_ERR_TEMPERATURE_HIGH_BIT);
    lua_setfield(L, -2, "ERRTEMPERATUREHIGHBIT");

    lua_pushinteger(L, EVSE_ERR_TEMPERATURE_FAULT_BIT);
    lua_setfield(L, -2, "ERRTEMPERATUREFAULTBIT");

    lua_newtable(L);
    lua_setfield(L, -2, "__drivers");

    return 1;
}
