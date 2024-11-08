#include "l_modbus_lib.h"

#include <esp_log.h>

#include "lauxlib.h"
#include "lua.h"

#define BUF_SIZE 256

// @todo

// static const char* TAG = "l_modbus";

// static int l_get_available(lua_State* L)
// {
    
//     return 0;
// }

// static int l_read(lua_State* L)
// {
   
//     return 0;
// }

// static int l_write(lua_State* L)
// {
   
//     return 0;
// }

static const luaL_Reg lib[] = {
    // { "getavailable", l_get_available },
    { NULL, NULL },
};

int luaopen_modbus(lua_State* L)
{
    luaL_newlib(L, lib);

    return 1;
}
