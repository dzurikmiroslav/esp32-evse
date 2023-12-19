#include "mqtt_client.h"
#include "lua.h"
#include "lauxlib.h"

#include "l_mqtt_lib.h"

#include "esp_log.h"

typedef struct
{
    esp_mqtt_client_handle_t client;
    bool connected : 1;
    lua_State* L;
    int self_ref;
    int on_connect_ref;
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
        ESP_LOGI("event_handler", "connected!");
        lua_State* L = lua_newthread(userdata->L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->on_connect_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->self_ref);
        lua_call(L, 1, 0);

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

        userdata->L = L;
        userdata->client = client;
        userdata->connected = false;

        luaL_unref(L, LUA_REGISTRYINDEX, userdata->self_ref);
        lua_pushvalue(L, 1);
        userdata->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);

        userdata->on_connect_ref = LUA_NOREF;
        return 1;
    }
}

static int l_client_connect(lua_State* L)
{
    ESP_LOGI("lm", "connect");
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (esp_mqtt_client_register_event(userdata->client, ESP_EVENT_ANY_ID, event_handler, userdata) != ESP_OK) {
        luaL_error(L, "Cant register handler");
    }
    if (esp_mqtt_client_start(userdata->client) != ESP_OK) {
        luaL_error(L, "Cant start config");
    }

    return 0;
}

static int l_client_on(lua_State* L)
{
    ESP_LOGI("lm", "on");

    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    luaL_argcheck(L, lua_isstring(L, 2), 2, "Must be string");

    luaL_argcheck(L, lua_isfunction(L, 3), 3, "Must be function");

    static const char* const name[] = { "connect",   NULL };

    switch (luaL_checkoption(L, 2, NULL, name)) {
    case 0:
        luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_connect_ref);
        userdata->on_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        ESP_LOGI("lm", "on_connect_ref %d", userdata->on_connect_ref);
        break;
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
    {"on",          l_client_on},
    {NULL, NULL}
};

static const luaL_Reg client_metadata[] = {
    {"__index",     NULL},
    {"__gc",        l_client_gc},
    {"__close",     l_client_gc},
    // {"__tostring",  l_client_tostring},
    {NULL, NULL}
};

// static const luaL_Reg client_fields[] = {
//     {"connect",     l_client_connect},
//     {"on",          l_client_on},
//     {"__index",     NULL},
//     {"__gc",        l_client_gc},
//     {"__close",     l_client_gc},
//     // {"__tostring",  l_client_tostring},
//     {NULL, NULL}
// };

int luaopen_mqtt(lua_State* L)
{
    luaL_newlib(L, lib);

    luaL_newmetatable(L, "mqtt.client");
    luaL_setfuncs(L, client_metadata, 0);
    luaL_newlibtable(L, client_fields);
    luaL_setfuncs(L, client_fields, 0);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // luaL_newmetatable(L, "mqtt.client");
    // luaL_setfuncs(L, client_fields, 0);
    // lua_pushvalue(L, -1);
    // lua_setfield(L, -1, "__index");
    // lua_pop(L, 1);

    return 1;
}