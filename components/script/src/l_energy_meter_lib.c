#include "l_energy_meter_lib.h"

#include <esp_log.h>
#include <math.h>
#include <string.h>

#include "energy_meter.h"
#include "lauxlib.h"
#include "lua.h"

static int l_get_mode(lua_State* L)
{
    lua_pushinteger(L, energy_meter_get_mode());
    return 1;
}

static int l_get_ac_voltage(lua_State* L)
{
    lua_pushinteger(L, energy_meter_get_ac_voltage());
    return 1;
}

static int l_get_three_phases(lua_State* L)
{
    lua_pushboolean(L, energy_meter_is_three_phases());
    return 1;
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

static const luaL_Reg lib[] = {
    // states
    { "MODEDUMMY", NULL },
    { "MODECUR", NULL },
    { "MODECURVLT", NULL },
    // methods
    { "getmode", l_get_mode },
    { "getacvoltage", l_get_ac_voltage },
    { "getthreephases", l_get_three_phases },
    { "getpower", l_get_power },
    { "getchargingtime", l_get_charging_time },
    { "getsessiontime", l_get_session_time },
    { "getconsumption", l_get_consumption },
    { "getvoltage", l_get_voltage },
    { "getcurrent", l_get_current },
    { NULL, NULL },
};

int luaopen_energy_meter(lua_State* L)
{
    luaL_newlib(L, lib);

    lua_pushinteger(L, ENERGY_METER_MODE_DUMMY);
    lua_setfield(L, -2, "MODEDUMMY");

    lua_pushinteger(L, ENERGY_METER_MODE_CUR);
    lua_setfield(L, -2, "MODECUR");

    lua_pushinteger(L, ENERGY_METER_MODE_CUR_VLT);
    lua_setfield(L, -2, "MODECURVLT");

    return 1;
}