#include "l_mqtt_lib.h"

#include <esp_log.h>

#include "lauxlib.h"
#include "lua.h"
#include "mqtt_client.h"
#include "script_utils.h"

static const char* TAG = "l_mqtt";

typedef enum {
    CLIENT_STATUS_INIT,
    CLIENT_STATUS_BEFORE_CONNECT,
    CLIENT_STATUS_CONNECTED,
    CLIENT_STATUS_DISCONNECTED
} client_status_t;

typedef struct {
    esp_mqtt_client_handle_t client;
    client_status_t client_status : 8;
    lua_State* L;
    int handler_table_ref;
} client_userdata_t;

static bool topic_match_pattern(const char* topic, const char* pattern)
{
    const char* pattern_ptr = pattern;
    const char* topic_ptr = topic;

    while (*pattern_ptr && *topic_ptr) {
        if (*pattern_ptr == '+') {
            pattern_ptr++;
            while (*topic_ptr && *topic_ptr != '/') {
                topic_ptr++;
            }
            if (*pattern_ptr == '/') pattern_ptr++;
            if (*topic_ptr == '/') topic_ptr++;
            continue;
        }
        if (*pattern_ptr == '#') {
            return true;
        }
        if (*pattern_ptr != *topic_ptr) {
            return false;
        }
        pattern_ptr++;
        topic_ptr++;
    }

    return (*pattern_ptr == *topic_ptr) || (*pattern_ptr == '#');
}

static void handle_event_data(client_userdata_t* userdata, esp_mqtt_event_handle_t event)
{
    char topic[128];
    size_t topic_len = event->topic_len < sizeof(topic) - 1 ? event->topic_len : sizeof(topic) - 1;
    memcpy(topic, event->topic, topic_len);
    topic[topic_len] = '\0';

    lua_State* L = userdata->L;

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->handler_table_ref);

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        const char* pattern = lua_tostring(L, -2);

        if (topic_match_pattern(topic, pattern)) {
            lua_pushlstring(L, event->topic, event->topic_len);
            lua_pushlstring(L, event->data, event->data_len);
            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                lua_writestring(err, strlen(err));
                lua_writeline();
            }
        } else {
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
}

static void event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    client_userdata_t* userdata = handler_args;
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        userdata->client_status = CLIENT_STATUS_CONNECTED;
        break;
    case MQTT_EVENT_DISCONNECTED:
        // TODO handle also MQTT_EVENT_ERROR ?
        userdata->client_status = CLIENT_STATUS_DISCONNECTED;
        break;
    case MQTT_EVENT_DATA:
        xSemaphoreTake(script_mutex, portMAX_DELAY);
        handle_event_data(userdata, event);
        xSemaphoreGive(script_mutex);
        break;
    default:
        break;
    }
}

static int l_client(lua_State* L)
{
    esp_mqtt_client_config_t cfg = { 0 };
    cfg.broker.address.uri = luaL_checkstring(L, 1);
    if (!lua_isnoneornil(L, 2)) {
        cfg.credentials.username = luaL_checkstring(L, 2);
    }
    if (!lua_isnoneornil(L, 3)) {
        cfg.credentials.authentication.password = luaL_checkstring(L, 3);
    }

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
    if (client == NULL) {
        luaL_error(L, "invalid params");
    } else {
        client_userdata_t* userdata = (client_userdata_t*)lua_newuserdatauv(L, sizeof(client_userdata_t), 0);
        luaL_setmetatable(L, "mqtt.client");

        userdata->L = L;
        userdata->client = client;
        userdata->client_status = CLIENT_STATUS_INIT;
        lua_newtable(L);
        userdata->handler_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    return 1;
}

static int l_client_connect_wait(lua_State* L, int status, lua_KContext ctx)
{
    client_userdata_t* userdata = (client_userdata_t*)ctx;

    if (userdata->client_status == CLIENT_STATUS_BEFORE_CONNECT) {
        return lua_yieldk(L, 0, (lua_KContext)userdata, l_client_connect_wait);
    } else {
        lua_pushboolean(L, userdata->client_status == CLIENT_STATUS_CONNECTED);
        return 1;
    }
}

static int l_client_connect(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (userdata->client_status == CLIENT_STATUS_INIT) {
        if (esp_mqtt_client_register_event(userdata->client, ESP_EVENT_ANY_ID, event_handler, userdata) != ESP_OK) {
            luaL_error(L, "cant register handler");
        }
        if (esp_mqtt_client_start(userdata->client) != ESP_OK) {
            luaL_error(L, "cant start config");
        }
        userdata->client_status = CLIENT_STATUS_BEFORE_CONNECT;
    }

    return lua_yieldk(L, 0, (lua_KContext)userdata, l_client_connect_wait);
}

static int l_client_disconnect(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (userdata->client != NULL) {
        if (esp_mqtt_client_disconnect(userdata->client) != ESP_OK) {
            ESP_LOGW(TAG, "Disconnect error");
        }
    }
    userdata->client_status = CLIENT_STATUS_INIT;

    return 0;
}

static int l_client_subscribe(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    const char* topic = luaL_checkstring(L, 2);

    luaL_checktype(L, 3, LUA_TFUNCTION);

    if (esp_mqtt_client_subscribe_single(userdata->client, topic, 0) < 0) {
        luaL_error(L, "cant subscribe");
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->handler_table_ref);

    lua_pushvalue(L, 3);
    lua_setfield(L, -2, topic);

    lua_pop(L, 1);

    return 0;
}

static int l_client_unsubscribe(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    const char* topic = luaL_checkstring(L, 2);
    if (esp_mqtt_client_unsubscribe(userdata->client, topic) < 0) {
        ESP_LOGW(TAG, "Cant unsubscribe");
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, userdata->handler_table_ref);

    lua_pushnil(L);
    lua_setfield(L, -2, topic);

    lua_pop(L, 1);

    return 0;
}

static int l_client_publish(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (userdata->client_status == CLIENT_STATUS_CONNECTED) {
        const char* topic = luaL_checkstring(L, 2);
        const char* data = luaL_checkstring(L, 3);
        int qos = 1;
        if (!lua_isnoneornil(L, 4)) {
            qos = luaL_checkinteger(L, 4);
        }
        int retry = 0;
        if (!lua_isnoneornil(L, 5)) {
            retry = luaL_checkinteger(L, 5);
        }

        esp_mqtt_client_publish(userdata->client, topic, data, 0, qos, retry);
    }

    return 0;
}

static int l_client_gc(lua_State* L)
{
    client_userdata_t* userdata = (client_userdata_t*)luaL_checkudata(L, 1, "mqtt.client");

    if (userdata->client != NULL) {
        esp_mqtt_client_destroy(userdata->client);
        userdata->client = NULL;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->handler_table_ref);
    userdata->handler_table_ref = LUA_NOREF;

    return 0;
}

static const luaL_Reg lib[] = {
    { "client", l_client },
    { NULL, NULL },
};

static const luaL_Reg client_fields[] = {
    { "connect", l_client_connect },         { "disconnect", l_client_disconnect }, { "subscribe", l_client_subscribe },
    { "unsubscribe", l_client_unsubscribe }, { "publish", l_client_publish },       { NULL, NULL },
};

static const luaL_Reg client_metadata[] = {
    { "__index", NULL },
    { "__gc", l_client_gc },
    { NULL, NULL },
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