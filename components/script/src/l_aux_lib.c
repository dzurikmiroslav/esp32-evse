#include "l_aux_lib.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "aux_io.h"
#include "lauxlib.h"
#include "lua.h"

static int l_write(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    bool value = lua_toboolean(L, 2);

    if (aux_write(name, value) != ESP_OK) {
        luaL_error(L, "unknow output name");
    }

    return 0;
}

static int l_read(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    bool value;
    if (aux_read(name, &value) == ESP_OK) {
        lua_pushboolean(L, value);
    } else {
        luaL_error(L, "unknow input name");
    }

    return 1;
}

static int l_analog_read(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    int value;
    if (aux_analog_read(name, &value) == ESP_OK) {
        lua_pushinteger(L, value);
    } else {
        luaL_error(L, "unknow input name");
    }

    return 1;
}

static const luaL_Reg lib[] = {
    { "write", l_write },
    { "read", l_read },
    { "analogread", l_analog_read },
    { NULL, NULL },
};

int luaopen_aux(lua_State* L)
{
    luaL_newlib(L, lib);

    return 1;
}