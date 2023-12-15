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
} mqtt_client_ctx_t;


static int l_client(lua_State* L)
{
    int argc = lua_gettop(L);

    luaL_argcheck(L, lua_isstring(L, 1), 1, "Must be string");
    if (argc > 1) {
        luaL_argcheck(L, lua_isstring(L, 2), 2, "Must be string");
    }
    if (argc > 2) {
        luaL_argcheck(L, lua_isstring(L, 3), 3, "Must be string");
    }

    esp_mqtt_client_config_t cfg = { 0 };
    cfg.broker.address.uri = lua_tostring(L, 1);
    if (argc > 1) {
        cfg.credentials.username = lua_tostring(L, 2);
    }
    if (argc > 2) {
        cfg.credentials.authentication.password = lua_tostring(L, 3);
    }

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    if (client == NULL) {
        return luaL_error(L, "Invalid config");
    } else {
        mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)malloc(sizeof(mqtt_client_ctx_t));
        ctx->client = client;
        ctx->connected = false;
        // var_setnil(&ctx->on_connect);
        // var_setnil(&ctx->on_message);

        // be_newcomobj(vm, ctx, &be_commonobj_destroy_generic);
        // be_setmember(vm, 1, ".p");

        // be_return(vm);
    }

    return 0;
}

static const luaL_Reg lib[] = {
    // methods
    {"client",            l_client},
    {NULL, NULL}
};


int luaopen_mqtt(lua_State* L)
{
    luaL_newlib(L, lib);
    return 1;
}