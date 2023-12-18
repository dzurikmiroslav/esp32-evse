#include "mqtt_client.h"
#include "lua.h"
#include "lauxlib.h"

#include "l_mqtt_lib.h"

#include "esp_log.h"

typedef struct
{
    esp_mqtt_client_handle_t client;
    bool connected : 1;
    // bvalue on_connect;
    // bvalue on_message;
} client_userdata_t;


static void event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    client_userdata_t* userdata = handler_args;
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        userdata->connected = true;
        // if (var_isfunction(&ctx->on_connect)) {
        //     xSemaphoreTake(script_mutex, portMAX_DELAY);
        //     bvalue* top = be_incrtop(script_vm);
        //     *top = ctx->on_connect;

        //     script_watchdog_reset();
        //     int ret = be_pcall(script_vm, 0);
        //     script_watchdog_disable();
        //     script_handle_result(ret);

        //     be_pop(script_vm, 1);
        //     xSemaphoreGive(script_mutex);
        // }
        break;
    case MQTT_EVENT_DISCONNECTED:
        userdata->connected = false;
        break;
    case MQTT_EVENT_DATA:
        // if (var_isfunction(&ctx->on_message)) {
        //     xSemaphoreTake(script_mutex, portMAX_DELAY);
        //     bvalue* top = be_incrtop(script_vm);
        //     *top = ctx->on_message;
        //     be_pushnstring(script_vm, event->topic, event->topic_len);
        //     be_pushnstring(script_vm, event->data, event->data_len);

        //     script_watchdog_reset();
        //     int ret = be_pcall(script_vm, 2);
        //     script_watchdog_disable();
        //     script_handle_result(ret);

        //     be_pop(script_vm, 3);
        //     xSemaphoreGive(script_mutex);
        // }
        break;
    default:
        break;
    }
}

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
        client_userdata_t* userdata = (client_userdata_t*)lua_newuserdatauv(L, sizeof(client_userdata_t), 0);
        luaL_setmetatable(L, "mqtt.client");

        userdata->client = client;
        userdata->connected = false;
        // var_setnil(&ctx->on_connect);
        // var_setnil(&ctx->on_message);

        // be_newcomobj(vm, ctx, &be_commonobj_destroy_generic);
        // be_setmember(vm, 1, ".p");

        // be_return(vm);
        return 1;
    }
}

static int l_client_connect(lua_State* L)
{
    ESP_LOGI("lm", "connect");
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (esp_mqtt_client_register_event(userdata->client, ESP_EVENT_ANY_ID, event_handler, userdata) != ESP_OK) {
         luaL_error(L, "Ccant register handler");
    }
    if (esp_mqtt_client_start(userdata->client) != ESP_OK) {
         luaL_error(L, "Cant start config");
    }

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

static const luaL_Reg client_fields[] = {
    {"connect",     l_client_connect},
    {NULL, NULL}
};


static const luaL_Reg client_metadata[] = {
    {"__index",     NULL},
    {"__gc",        l_client_gc},
    {"__close",     l_client_gc},
    // {"__tostring",  l_client_tostring},
     {NULL, NULL}
};


int luaopen_mqtt(lua_State* L)
{
    luaL_newlib(L, lib);

    luaL_newmetatable(L, "mqtt.client");
    luaL_setfuncs(L, client_metadata, 0);
    luaL_newlibtable(L, client_fields);
    luaL_setfuncs(L, client_fields, 0);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

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