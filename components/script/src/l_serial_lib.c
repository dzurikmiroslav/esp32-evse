#include "l_serial_lib.h"

#include <esp_log.h>

#include "lauxlib.h"
#include "lua.h"
#include "serial_script.h"

#define BUF_SIZE 256

static const char* TAG = "l_serial";

static bool is_opened;

static int l_open(lua_State* L)
{
    ESP_LOGW(TAG, "l_open");

    if (!serial_script_is_available() || is_opened) {
        lua_pushnil(L);
    } else {
        is_opened = true;
        lua_newtable(L);
        luaL_setmetatable(L, "serial.port");
    }

    return 1;
}

static int l_port_read(lua_State* L)
{
    ESP_LOGW(TAG, "l_port_read");

    char buf[BUF_SIZE];
    size_t len = BUF_SIZE;

    if (serial_script_read(buf, &len) != ESP_OK) {
        luaL_error(L, "serial error");
    }

    if (!len) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, buf, len);
    }

    return 1;
}

static int l_port_write(lua_State* L)
{
    ESP_LOGW(TAG, "l_port_write");

    size_t len;
    const char* buf;
    char buf_num;

    if (lua_isnumber(L, 2)) {
        buf_num = lua_tonumber(L, 2);
        len = 1;
        buf = &buf_num;
    } else if (lua_isstring(L, 2)) {
        buf = lua_tolstring(L, 2, &len);
    } else {
        luaL_argerror(L, 2, "argument must be number or string");
        return 0;
    }

    if (serial_script_write(buf, len) != ESP_OK) {
        luaL_error(L, "serial error");
    }

    return 0;
}

static int l_port_flush(lua_State* L)
{
    ESP_LOGW(TAG, "l_port_flush");

    if (serial_script_flush() != ESP_OK) {
        luaL_error(L, "serial error");
    }

    return 0;
}

static int l_port_gc(lua_State* L)
{
    ESP_LOGW(TAG, "l_port_gc");

    is_opened = false;

    return 0;
}

static const luaL_Reg lib[] = {
    { "open", l_open },
    { NULL, NULL },
};

static const luaL_Reg port_fields[] = {
    { "write", l_port_write },
    { "read", l_port_read },
    { "flush", l_port_flush },
    { NULL, NULL },
};

static const luaL_Reg port_metadata[] = {
    { "__index", NULL },
    { "__gc", l_port_gc },
    { NULL, NULL },
};

int luaopen_serial(lua_State* L)
{
    is_opened = false;

    luaL_newlib(L, lib);

    luaL_newmetatable(L, "serial.port");
    luaL_setfuncs(L, port_metadata, 0);
    luaL_newlibtable(L, port_fields);
    luaL_setfuncs(L, port_fields, 0);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    return 1;
}
