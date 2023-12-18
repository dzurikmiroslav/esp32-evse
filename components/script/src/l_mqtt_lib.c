#include "mqtt_client.h"
#include "lua.h"
#include "lauxlib.h"

#include "l_mqtt_lib.h"

typedef struct
{
    esp_mqtt_client_handle_t client;
    bool connected : 1;
    // bvalue on_connect;
    // bvalue on_message;
} client_userdata_t;


static int l_client(lua_State* L)
{
    // int argc = lua_gettop(L);

    // luaL_argcheck(L, lua_isstring(L, 1), 1, "Must be string");
    // if (argc > 1) {
    //     luaL_argcheck(L, lua_isstring(L, 2), 2, "Must be string");
    // }
    // if (argc > 2) {
    //     luaL_argcheck(L, lua_isstring(L, 3), 3, "Must be string");
    // }

    // esp_mqtt_client_config_t cfg = { 0 };
    // cfg.broker.address.uri = lua_tostring(L, 1);
    // if (argc > 1) {
    //     cfg.credentials.username = lua_tostring(L, 2);
    // }
    // if (argc > 2) {
    //     cfg.credentials.authentication.password = lua_tostring(L, 3);
    // }

    // esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    // if (client == NULL) {
    //     return luaL_error(L, "Invalid config");
    // } else {
    //     client_userdata_t* userdata = (client_userdata_t*)lua_newuserdatauv(L, sizeof(client_userdata_t), 0);
    //     luaL_setmetatable(L, "mqtt.client");

    //     userdata->client = client;
    //     userdata->connected = false;
    //     // var_setnil(&ctx->on_connect);
    //     // var_setnil(&ctx->on_message);

    //     // be_newcomobj(vm, ctx, &be_commonobj_destroy_generic);
    //     // be_setmember(vm, 1, ".p");

    //     // be_return(vm);
    //     return 1;
    // }
    return 0;
}

static int l_client_connect(lua_State* L)
{

    return 0;
}

static int l_client_gc(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (userdata->client != NULL) {
        esp_mqtt_client_destroy(userdata->client);
    }

    return 0;
}

static const luaL_Reg lib[] = {
    {"client",      l_client},
    {NULL, NULL}
};

// static const luaL_Reg client_fields[] = {
//     {"connect",     l_client_connect},
//     {NULL, NULL}
// };


// static const luaL_Reg client_metadata[] = {
//     {"__index",     NULL},
//     {"__gc",        l_client_gc},
//     {"__close",     l_client_gc},
//     // {"__tostring",  l_client_tostring},
//      {NULL, NULL}
// };


int luaopen_mqtt(lua_State* L)
{
    luaL_newlib(L, lib);

    // luaL_newmetatable(L, "mqtt.client");
    // luaL_setfuncs(L, client_metadata, 0);
    // luaL_newlibtable(L, client_fields);
    // luaL_setfuncs(L, client_fields, 0);
    // lua_setfield(L, -2, "__index");
    // lua_pop(L, 1);

    return 1;
}

/*
main:
--evse.adddriver( require ("tb"))

local ok, mod = pcall(require, "tb")

local counter = 0
evse.adddriver({
    every1s = function()
        print(evse, mqtt)
        print("TB every 1s", counter)
        counter = counter + 1
    end
})

--print('count driver ', #evse.__drivers)

print (evse)

tb:
local counter = 0

-- local mqtt = require("mqtt")

return {
    every1s = function()
        print(evse == nil)
        -- print("TB every 1s", counter)
        counter = counter + 1
    end
}

*/