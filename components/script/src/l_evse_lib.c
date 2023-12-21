#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys/queue.h"
#include "lua.h"
#include "lauxlib.h"

#include "l_evse_lib.h"
#include "evse.h"
#include "energy_meter.h"
#include "temp_sensor.h"

#include "esp_log.h"

typedef struct driver_entry_s {
    SLIST_ENTRY(driver_entry_s) entries;
    int ref;
} driver_entry_t;

typedef struct
{
    TickType_t tick_100ms;
    TickType_t tick_250ms;
    TickType_t tick_1s;
    SLIST_HEAD(drivers_head, driver_entry_s) drivers;
} evse_ctx_t;

static evse_ctx_t* ctx = NULL;

void l_evse_init(void)
{
    ctx = (evse_ctx_t*)malloc(sizeof(evse_ctx_t));
    ctx->tick_100ms = 0;
    ctx->tick_250ms = 0;
    ctx->tick_1s = 0;
    SLIST_INIT(&ctx->drivers);
}

void l_evse_deinit(void)
{
    //todo clean drivers

    free((void*)ctx);
    ctx = NULL;
}

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

static int l_process(lua_State* L)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t prev;

    lua_getglobal(L, "evse");

    lua_getfield(L, -1, "__tick100ms");
    prev = lua_tointeger(L, -1);
    lua_pop(L, 1);
    bool every_100ms = now - prev >= pdMS_TO_TICKS(100);

    lua_getfield(L, -1, "__tick250ms");
    prev = lua_tointeger(L, -1);
    lua_pop(L, 1);
    bool every_250ms = now - prev >= pdMS_TO_TICKS(250);

    lua_getfield(L, -1, "__tick1s");
    prev = lua_tointeger(L, -1);
    lua_pop(L, 1);
    bool every_1s = now - prev >= pdMS_TO_TICKS(1000);

    if (every_100ms) {
        lua_pushinteger(L, now);
        lua_setfield(L, -2, "__tick100ms");
    }
    if (every_250ms) {
        lua_pushinteger(L, now);
        lua_setfield(L, -2, "__tick250ms");
    }
    if (every_1s) {
        lua_pushinteger(L, now);
        lua_setfield(L, -2, "__tick1s");
    }

    lua_getfield(L, -1, "__drivers");
    int len = lua_rawlen(L, -1);
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, -1, i);
        if (lua_istable(L, -1)) {
            if (every_100ms) {
                call_field_event(L, "every100ms");
            }
            if (every_250ms) {
                call_field_event(L, "every250ms");
            }
            if (every_1s) {
                call_field_event(L, "every1s");
            }
        } else {
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    return 0;
}

static void driver_call_event(lua_State* L, const char* method)
{
    lua_getfield(L, -1, method);

    if (lua_isfunction(L, -1)) {
        be_pushvalue(vm, -2);

        int ret = be_pcall(vm, 1);

        
    }
    be_pop(vm, 1);
}

void l_evse_process(lua_State* L)
{
    TickType_t now = xTaskGetTickCount();

    bool every_100ms = false;
    if (now - ctx->tick_100ms >= pdMS_TO_TICKS(100)) {
        ctx->tick_100ms = now;
        every_100ms = true;
    }

    bool every_250ms = false;
    if (now - ctx->tick_250ms >= pdMS_TO_TICKS(250)) {
        ctx->tick_250ms = now;
        every_250ms = true;
    }

    bool every_1s = false;
    if (now - ctx->tick_1s >= pdMS_TO_TICKS(1000)) {
        ctx->tick_1s = now;
        every_1s = true;
    }

    driver_entry_t* driver;
    SLIST_FOREACH(driver, &ctx->drivers, entries) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, driver->ref);
        driver_call_event(L, "loop");
        if (every_100ms) {
            driver_call_event(L, "every100ms");
        }
        if (every_250ms) {
            driver_call_event(L, "every250ms");
        }
        if (every_1s) {
            driver_call_event(L, "every1s");
        }

        lua_pop(L, 1);
    }
}

static int l_add_driver(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    driver_entry_t* entry = (driver_entry_t*)malloc(sizeof(driver_entry_t));
    entry->ref = luaL_ref(L, LUA_REGISTRYINDEX);
    SLIST_INSERT_HEAD(&ctx->drivers, entry, entries);

    // luaL_argcheck(L, lua_istable(L, 1), 1, "Must be table");

    // lua_getglobal(L, "evse");
    // lua_getfield(L, -1, "__drivers");
    // lua_pushvalue(L, 1);
    // lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);

    return 0;
}

static int l_gc(lua_State* L)
{
    ESP_LOGW("levse", "l_gc");

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

    // private methods
    {"__process",           l_process},
    // private fields
    {"__drivers",           NULL},
    {"__tick100ms",         NULL},
    {"__tick250ms",         NULL},
    {"__tick1s",            NULL},
    {NULL, NULL}
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

    lua_newtable(L);
    lua_setfield(L, -2, "__drivers");

    TickType_t now = xTaskGetTickCount();

    lua_pushinteger(L, now);
    lua_setfield(L, -2, "__tick100ms");

    lua_pushinteger(L, now);
    lua_setfield(L, -2, "__tick250ms");

    lua_pushinteger(L, now);
    lua_setfield(L, -2, "__tick1s");

    return 1;
}
