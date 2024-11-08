#include "l_serial_lib.h"

#include <esp_log.h>

#include "lauxlib.h"
#include "lua.h"
#include "serial_script.h"

#define BUF_SIZE 256

// static const char* TAG = "l_serial";

static int l_get_available(lua_State* L)
{
    lua_pushboolean(L, serial_script_is_available());
    return 1;
}

static int l_read(lua_State* L)
{
    char buf[BUF_SIZE];
    size_t len = BUF_SIZE;
    uint32_t timeout = 0;
    if (!lua_isnoneornil(L, 1)) {
        timeout = luaL_checkinteger(L, 1);
    }

    if (serial_script_read(buf, &len, timeout) != ESP_OK) {
        luaL_error(L, "serial error");
    }

    if (!len) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, buf, len);
    }

    return 1;
}

static int l_write(lua_State* L)
{
    size_t len;
    const char* buf;
    char buf_num;

    if (lua_isnumber(L, 1)) {
        buf_num = lua_tonumber(L, 1);
        len = 1;
        buf = &buf_num;
    } else if (lua_isstring(L, 1)) {
        buf = lua_tolstring(L, 1, &len);
    } else {
        luaL_error(L, "argument must be number or string");
        return 0;
    }

    serial_script_write(buf, len);

    return 0;
}

static const luaL_Reg lib[] = {
    { "getavailable", l_get_available },
    { "read", l_read },
    { "write", l_write },
    { NULL, NULL },
};

int luaopen_serial(lua_State* L)
{
    luaL_newlib(L, lib);

    return 1;
}
