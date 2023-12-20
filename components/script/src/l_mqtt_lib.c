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
    int on_message_ref;
} client_userdata_t;


static void event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    client_userdata_t* userdata = handler_args;
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        userdata->connected = true;
        lua_State* L = lua_newthread(userdata->L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->on_connect_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->self_ref);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            lua_writestring(err, strlen(err));
            lua_writeline();
            lua_pop(L, 1);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI("event_handler", "disconnected!");
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

    esp_mqtt_client_config_t cfg = { 0 };
    cfg.broker.address.uri = luaL_checkstring(L, 1);
    if (argc > 1) {
        cfg.credentials.username = luaL_checkstring(L, 2);
    }
    if (argc > 2) {
        cfg.credentials.authentication.password = luaL_checkstring(L, 3);
    }

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    if (client == NULL) {
        return luaL_error(L, "Invalid params");
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
        userdata->on_message_ref = LUA_NOREF;
        return 1;
    }
}

static int l_client_connect(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (esp_mqtt_client_register_event(userdata->client, ESP_EVENT_ANY_ID, event_handler, userdata) != ESP_OK) {
        luaL_error(L, "Cant register handler");
    }
    if (esp_mqtt_client_start(userdata->client) != ESP_OK) {
        luaL_error(L, "Cant start config");
    }

    return 0;
}

static int l_client_disconnect(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (userdata->client != NULL) {
        if (esp_mqtt_client_disconnect(userdata->client) != ESP_OK) {
            luaL_error(L, "Disconnect error");
        }
    }

    return 0;
}

static int l_client_set_onconnect(lua_State* L)
{
    int argc = lua_gettop(L);

    luaL_argexpected(L, lua_isfunction(L, 2), 2, "function");

    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_connect_ref);
    userdata->on_connect_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

static int l_client_subscribe(lua_State* L)
{
    int argc = lua_gettop(L);

    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    const char* topic = luaL_checkstring(L, 2);
    int qos = 0;
    if (argc > 2) {
        qos = luaL_checkinteger(L, 3);
    }

    if (userdata->connected) {
        int sub_id = esp_mqtt_client_subscribe_single(userdata->client, topic, qos);
        lua_pushinteger(L, sub_id);
    } else {
        lua_pushinteger(L, -1);
    }

    return 1;
}

static int l_client_gc(lua_State* L)
{
    ESP_LOGW("lm", "l_client_gc");

    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (userdata->client != NULL) {
        esp_mqtt_client_destroy(userdata->client);
        userdata->client = NULL;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->self_ref);
    userdata->self_ref = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_connect_ref);
    userdata->on_connect_ref = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_message_ref);
    userdata->on_message_ref = LUA_NOREF;

    return 0;
}

static const luaL_Reg lib[] = {
    {"client",      l_client},
    {NULL, NULL}
};

static const luaL_Reg client_fields[] = {
    {"connect",         l_client_connect},
    {"disconnect",      l_client_disconnect},
    {"subscribe",       l_client_subscribe},
    {"setonconnect",    l_client_set_onconnect},
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