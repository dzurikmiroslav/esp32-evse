#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "script.h"
#include "script_utils.h"
#include "output_buffer.h"
#include "l_evse_lib.h"
#include "l_mqtt_lib.h"
#include "l_json_lib.h"
#include "l_aux_lib.h"
#include "l_board_config_lib.h"

#define START_TIMEOUT           1000
#define SHUTDOWN_TIMEOUT        1000
#define HEARTBEAT_THRESHOLD     5
#define OUTPUT_BUFFER_SIZE      4096

#define NVS_NAMESPACE           "script"
#define NVS_ENABLED             "enabled"

static const char* TAG = "script";

static nvs_handle nvs;

static TaskHandle_t script_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

static lua_State* L = NULL;

static output_buffer_t* output_buffer = NULL;

static SemaphoreHandle_t output_mutex;

SemaphoreHandle_t script_mutex = NULL;

static void script_task_func(void* param)
{
    lua_writestring(LUA_COPYRIGHT, strlen(LUA_COPYRIGHT));
    lua_writeline();

    L = luaL_newstate();

    luaL_openlibs(L);

    luaL_requiref(L, "evse", luaopen_evse, 1);
    lua_pop(L, 1);

    luaL_requiref(L, "mqtt", luaopen_mqtt, 0);
    lua_pop(L, 1);

    luaL_requiref(L, "json", luaopen_json, 0);
    lua_pop(L, 1);

    luaL_requiref(L, "aux", luaopen_aux, 0);
    lua_pop(L, 1);

    luaL_requiref(L, "boardconfig", luaopen_board_config, 0);
    lua_pop(L, 1);

    lua_gc(L, LUA_GCSETPAUSE, 110);
    lua_gc(L, LUA_GCSETSTEPMUL, 200);

    const char* loading_msg = "loading file '/data/init.lua'...";
    lua_writestring(loading_msg, strlen(loading_msg));
    lua_writeline();

    if (luaL_dofile(L, "/data/init.lua") != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        lua_writestring(err, strlen(err));
        lua_writeline();
        lua_pop(L, 1);
    }

    while (true) {
        if (shutdown_sem != NULL) {
            break;
        }
        xSemaphoreTake(script_mutex, portMAX_DELAY);
        l_evse_process(L);
        xSemaphoreGive(script_mutex);

        int top = lua_gettop(L);
        if (top != 0) {
            ESP_LOGW(TAG, "top is %d, %d", top, lua_type(L, top));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    lua_close(L);
    L = NULL;

    if (shutdown_sem) {
        xSemaphoreGive(shutdown_sem);
    }
    vTaskDelete(NULL);
}

static void script_stop(void)
{
    if (script_task) {
        ESP_LOGI(TAG, "Stopping script");
        shutdown_sem = xSemaphoreCreateBinary();

        if (!xSemaphoreTake(shutdown_sem, pdMS_TO_TICKS(SHUTDOWN_TIMEOUT))) {
            ESP_LOGE(TAG, "Task stop timeout, will be force stoped");
            vTaskDelete(script_task);
            if (L != NULL) {
                xSemaphoreGive(script_mutex);
                lua_close(L);
                L = NULL;
            }
        }

        vSemaphoreDelete(shutdown_sem);
        shutdown_sem = NULL;
        script_task = NULL;
    }
}

void script_start(void)
{
    if (!script_task) {
        ESP_LOGI(TAG, "Starting script");
        xTaskCreate(script_task_func, "script_task", 6 * 1024, NULL, 5, &script_task);
    }
}

void script_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    output_mutex = xSemaphoreCreateMutex();
    output_buffer = output_buffer_create(OUTPUT_BUFFER_SIZE);
    script_mutex = xSemaphoreCreateMutex();

    if (script_is_enabled()) {
        script_start();
    }
}

void script_output_append_buf(const char* str, uint16_t len)
{
    xSemaphoreTake(output_mutex, portMAX_DELAY);

    output_buffer_append_buf(output_buffer, str, len);

    xSemaphoreGive(output_mutex);
}

uint16_t script_output_count(void)
{
    return output_buffer->count;
}

bool script_output_read(uint16_t* index, char** str, uint16_t* len)
{
    xSemaphoreTake(output_mutex, portMAX_DELAY);

    bool has_next = output_buffer_read(output_buffer, index, str, len);
    xSemaphoreGive(output_mutex);

    return has_next;
}

void script_reload(void)
{
    if (script_is_enabled()) {
        script_stop();
        script_start();
    }
}

void script_set_enabled(bool enabled)
{
    nvs_set_u8(nvs, NVS_ENABLED, enabled);

    nvs_commit(nvs);

    if (enabled) {
        script_start();
    } else {
        script_stop();
    }
}

bool script_is_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}

uint8_t script_driver_get_count(void)
{
    xSemaphoreTake(script_mutex, portMAX_DELAY);

    uint8_t count = l_evse_get_driver_count(L);

    xSemaphoreGive(script_mutex);

    return count;
}

// lua args: config table, config meta 
static cJSON* l_read_config_keys(lua_State* L)
{
    cJSON* json = cJSON_CreateArray();

    int count = lua_rawlen(L, -1);
    for (int i = 1; i <= count; i++) {
        lua_rawgeti(L, -1, i);

        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "type");
            const char* type = lua_tostring(L, -1);
            lua_pop(L, 1);

            if (strcmp("string", type) != 0 && strcmp("number", type) != 0 && strcmp("boolean", type) != 0) {
                continue;
            }

            lua_getfield(L, -1, "key");
            const char* key = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "name");
            const char* name = lua_tostring(L, -1);
            lua_pop(L, 1);

            cJSON* value_json = NULL;
            lua_getfield(L, -3, key);
            if (lua_isnil(L, -1)) {
                value_json = cJSON_CreateNull();
            } else {
                if (strcmp("string", type) == 0) {
                    value_json = cJSON_CreateString(lua_tostring(L, -1));
                } else if (strcmp("number", type) == 0) {
                    value_json = cJSON_CreateNumber(lua_tonumber(L, -1));
                } else {
                    value_json = cJSON_CreateBool(lua_toboolean(L, -1));
                }
            }
            lua_pop(L, 1);

            cJSON* item_json = cJSON_CreateObject();
            cJSON_AddStringToObject(item_json, "key", key);
            cJSON_AddStringToObject(item_json, "type", type);
            cJSON_AddStringToObject(item_json, "name", name);
            cJSON_AddItemToObject(item_json, "value", value_json);

            cJSON_AddItemToArray(json, item_json);
        }

        lua_pop(L, 1);
    }

    return json;
}

cJSON* script_driver_read_config(uint8_t index)
{
    cJSON* json = cJSON_CreateObject();

    xSemaphoreTake(script_mutex, portMAX_DELAY);

    l_evse_get_driver(L, index);

    lua_getfield(L, -1, "name");
    if (lua_isstring(L, -1)) {
        cJSON_AddStringToObject(json, "name", lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "readconfig");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
            if (lua_istable(L, -1)) {
                lua_getfield(L, -2, "config");

                cJSON_AddItemToObject(json, "config", l_read_config_keys(L));

                lua_pop(L, 1);
            } else {
                const char* err = "readconfig not return table";
                lua_writestring(err, strlen(err));
                lua_writeline();
            }
            lua_pop(L, 1);
        } else {
            const char* err = lua_tostring(L, -1);
            lua_writestring(err, strlen(err));
            lua_writeline();
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    xSemaphoreGive(script_mutex);

    return json;
}

static void l_read_config_keys( script_driver_t * driver)
{
    driver->config_entries_count = lua_rawlen(L, -1);
    

    for (uint8_t i = 1; i <= driver->config_entries_count; i++) {
        lua_rawgeti(L, -1, i);

        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "type");
            const char* type = lua_tostring(L, -1);
            lua_pop(L, 1);

            if (strcmp("string", type) != 0 && strcmp("number", type) != 0 && strcmp("boolean", type) != 0) {
                continue;
            }

            lua_getfield(L, -1, "key");
            const char* key = lua_tostring(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "name");
            const char* name = lua_tostring(L, -1);
            lua_pop(L, 1);

            cJSON* value_json = NULL;
            lua_getfield(L, -3, key);
            if (lua_isnil(L, -1)) {
                value_json = cJSON_CreateNull();
            } else {
                if (strcmp("string", type) == 0) {
                    value_json = cJSON_CreateString(lua_tostring(L, -1));
                } else if (strcmp("number", type) == 0) {
                    value_json = cJSON_CreateNumber(lua_tonumber(L, -1));
                } else {
                    value_json = cJSON_CreateBool(lua_toboolean(L, -1));
                }
            }
            lua_pop(L, 1);

            cJSON* item_json = cJSON_CreateObject();
            cJSON_AddStringToObject(item_json, "key", key);
            cJSON_AddStringToObject(item_json, "type", type);
            cJSON_AddStringToObject(item_json, "name", name);
            cJSON_AddItemToObject(item_json, "value", value_json);

            cJSON_AddItemToArray(json, item_json);
        }

        lua_pop(L, 1);
    }

    return json;
}


script_driver_t* script_driver_get(uint8_t index)
{    
    script_driver_t * driver = (script_driver_t*)malloc(sizeof(script_driver_t));
    
    xSemaphoreTake(script_mutex, portMAX_DELAY);

    l_evse_get_driver(L, index);

    lua_getfield(L, -1, "name");
    if (lua_isstring(L, -1)) {
        driver->name = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "readconfig");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
            if (lua_istable(L, -1)) {
                lua_getfield(L, -2, "config");

                cJSON_AddItemToObject(json, "config", l_read_config_keys(L));

                lua_pop(L, 1);
            } else {
                const char* err = "readconfig not return table";
                lua_writestring(err, strlen(err));
                lua_writeline();
            }
            lua_pop(L, 1);
        } else {
            const char* err = lua_tostring(L, -1);
            lua_writestring(err, strlen(err));
            lua_writeline();
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    xSemaphoreGive(script_mutex);

    return driver;
}

// lua args: config meta
void l_json_to_config_table(lua_State* L, cJSON* json)
{
    lua_newtable(L);

    int count = lua_rawlen(L, -2);
    for (int i = 1; i <= count; i++) {
        lua_rawgeti(L, -2, i);

        lua_getfield(L, -1, "type");
        const char* type = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (strcmp("string", type) != 0 && strcmp("number", type) != 0 && strcmp("boolean", type) != 0) {
            continue;
        }

        lua_getfield(L, -1, "key");
        const char* key = lua_tostring(L, -1);
        lua_pop(L, 1);

        cJSON* entry_json = NULL;
        cJSON_ArrayForEach(entry_json, json) {
            if (strcmp(key, cJSON_GetStringValue(cJSON_GetObjectItem(entry_json, "key"))) == 0) {
                cJSON* value_json = cJSON_GetObjectItem(entry_json, "value");
                if (cJSON_IsNull(value_json)) {
                    lua_pushnil(L);
                } else if (strcmp("string", type) == 0) {
                    lua_pushstring(L, cJSON_GetStringValue(value_json));
                } else if (strcmp("number", type) == 0) {
                    lua_pushnumber(L, cJSON_GetNumberValue(value_json));
                } else {
                    lua_pushboolean(L, cJSON_IsTrue(value_json));
                }
                lua_setfield(L, -2, key);

                break;
            }
        }

        lua_pop(L, 1);
    }
}

void script_driver_write_config(uint8_t index, cJSON* json)
{
    xSemaphoreTake(script_mutex, portMAX_DELAY);

    l_evse_get_driver(L, index);

    lua_getfield(L, -1, "writeconfig");
    if (lua_isfunction(L, -1)) {
        lua_getfield(L, -2, "config");
        l_json_to_config_table(L, json);
        lua_remove(L, -2);

        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            lua_writestring(err, strlen(err));
            lua_writeline();
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    lua_pop(L, 1);

    xSemaphoreGive(script_mutex);
}