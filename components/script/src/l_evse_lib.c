#include "l_evse_lib.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "energy_meter.h"
#include "evse.h"
#include "lauxlib.h"
#include "lua.h"
#include "temp_sensor.h"

typedef struct {
    TickType_t tick_100ms;
    TickType_t tick_250ms;
    TickType_t tick_1s;
    int drivers_ref;
} evse_userdata_t;

static int userdata_ref = LUA_NOREF;

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
    luaL_argcheck(L, lua_istable(L, 1), 1, "Must be table");

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata_ref);
    evse_userdata_t* userdata = lua_touserdata(L, -1);

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->drivers_ref);

    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);

    return 0;
}

static void call_field_event(lua_State* L, const char* event)
{
    lua_getfield(L, -1, event);
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            lua_writestring(err, strlen(err));
            lua_writeline();
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

void l_evse_process(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata_ref);
    evse_userdata_t* userdata = lua_touserdata(L, -1);

    TickType_t now = xTaskGetTickCount();

    bool every_100ms = false;
    if (now - userdata->tick_100ms >= pdMS_TO_TICKS(100)) {
        userdata->tick_100ms = now;
        every_100ms = true;
    }

    bool every_250ms = false;
    if (now - userdata->tick_250ms >= pdMS_TO_TICKS(250)) {
        userdata->tick_250ms = now;
        every_250ms = true;
    }

    bool every_1s = false;
    if (now - userdata->tick_1s >= pdMS_TO_TICKS(1000)) {
        userdata->tick_1s = now;
        every_1s = true;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->drivers_ref);

    int len = lua_rawlen(L, -1);
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, -1, i);

        call_field_event(L, "loop");
        if (every_100ms) {
            call_field_event(L, "every100ms");
        }
        if (every_250ms) {
            call_field_event(L, "every250ms");
        }
        if (every_1s) {
            call_field_event(L, "every1s");
        }

        lua_pop(L, 1);
    }

    lua_pop(L, 2);
}

uint8_t l_evse_get_driver_count(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata_ref);
    evse_userdata_t* userdata = lua_touserdata(L, -1);

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->drivers_ref);

    int len = lua_rawlen(L, -1);

    lua_pop(L, 2);

    return len;
}

void l_evse_get_driver(lua_State* L, uint8_t index)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata_ref);
    evse_userdata_t* userdata = lua_touserdata(L, -1);

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->drivers_ref);

    lua_rawgeti(L, -1, index + 1);

    lua_remove(L, -2);

    lua_remove(L, -2);
}

static const luaL_Reg lib[] = {
    // states
    { "STATEA", NULL },
    { "STATEB1", NULL },
    { "STATEB2", NULL },
    { "STATEC1", NULL },
    { "STATEC2", NULL },
    { "STATED1", NULL },
    { "STATED2", NULL },
    { "STATEE", NULL },
    { "STATEF", NULL },
    // error bits
    { "ERRPILOTFAULTBIT", NULL },
    { "ERRDIODESHORTBIT", NULL },
    { "ERRLOCKFAULTBIT", NULL },
    { "ERRUNLOCKFAULTBIT", NULL },
    { "ERRRCMTRIGGEREDBIT", NULL },
    { "ERRRCMSELFTESTFAULTBIT", NULL },
    { "ERRTEMPERATUREHIGHBIT", NULL },
    { "ERRTEMPERATUREFAULTBIT", NULL },
    // methods
    { "getstate", l_get_state },
    { "geterror", l_get_error },
    { "getenabled", l_get_enabled },
    { "setenabled", l_set_enabled },
    { "getavailable", l_get_available },
    { "setavailable", l_set_available },
    { "getchargingcurrent", l_get_charging_current },
    { "setchargingcurrent", l_set_charging_current },
    { "getpower", l_get_power },
    { "getchargingtime", l_get_charging_time },
    { "getsessiontime", l_get_session_time },
    { "getconsumption", l_get_consumption },
    { "getvoltage", l_get_voltage },
    { "getcurrent", l_get_current },
    { "getlowtemperature", l_get_low_temperature },
    { "gethightemperature", l_get_high_temperature },
    { "adddriver", l_add_driver },
    { NULL, NULL },
};

int luaopen_evse(lua_State* L)
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

    evse_userdata_t* userdata = (evse_userdata_t*)lua_newuserdatauv(L, sizeof(evse_userdata_t), 0);
    userdata_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    TickType_t now = xTaskGetTickCount();
    userdata->tick_100ms = now;
    userdata->tick_250ms = now;
    userdata->tick_1s = now;

    lua_newtable(L);
    userdata->drivers_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    return 1;
}