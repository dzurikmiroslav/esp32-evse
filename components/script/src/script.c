#include "script.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>

#include "l_aux_lib.h"
#include "l_board_config_lib.h"
#include "l_evse_lib.h"
#include "l_json_lib.h"
#include "l_mqtt_lib.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "output_buffer.h"
#include "script_utils.h"

#define START_TIMEOUT       10000
#define SHUTDOWN_TIMEOUT    1000
#define HEARTBEAT_THRESHOLD 5
#define OUTPUT_BUFFER_SIZE  4096

#define NVS_NAMESPACE "script"
#define NVS_ENABLED   "enabled"

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
                lua_close(L);
                L = NULL;
                xSemaphoreGive(script_mutex);
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
        xSemaphoreTake(script_mutex, portMAX_DELAY);

        script_stop();
        script_start();

        xSemaphoreGive(script_mutex);
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
    uint8_t count = 0;

    xSemaphoreTake(script_mutex, portMAX_DELAY);

    if (L != NULL) {
        count = l_evse_get_driver_count(L);
    }

    xSemaphoreGive(script_mutex);

    return count;
}

static void l_read_cfg_keys(script_driver_t* driver)
{
    driver->cfg_entries_count = lua_rawlen(L, -1);
    driver->cfg_entries = (script_driver_cfg_meta_entry_t*)malloc(sizeof(script_driver_cfg_meta_entry_t) * driver->cfg_entries_count);
    memset((void*)driver->cfg_entries, 0, sizeof(script_driver_cfg_meta_entry_t) * driver->cfg_entries_count);

    for (uint8_t i = 0; i < driver->cfg_entries_count; i++) {
        lua_rawgeti(L, -1, i + 1);

        if (lua_istable(L, -1)) {
            script_driver_cfg_meta_entry_t* cfg_ent = &driver->cfg_entries[i];
            lua_getfield(L, -1, "key");
            cfg_ent->key = strdup(lua_tostring(L, -1));
            lua_pop(L, 1);

            lua_getfield(L, -1, "name");
            cfg_ent->name = strdup(lua_tostring(L, -1));
            lua_pop(L, 1);

            lua_getfield(L, -1, "type");
            cfg_ent->type = script_str_to_driver_cfg_entry_type(lua_tostring(L, -1));
            lua_pop(L, 1);

            lua_getfield(L, -3, cfg_ent->key);
            if (!lua_isnil(L, -1)) {
                switch (cfg_ent->type) {
                case SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING:
                    cfg_ent->value.string = strdup(lua_tostring(L, -1));
                    break;
                case SCRIPT_DRIVER_CFG_ENTRY_TYPE_NUMBER:
                    cfg_ent->value.number = lua_tonumber(L, -1);
                    break;
                case SCRIPT_DRIVER_CFG_ENTRY_TYPE_BOOLEAN:
                    cfg_ent->value.boolean = lua_toboolean(L, -1);
                    break;
                default:
                }
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }
}

script_driver_t* script_driver_get(uint8_t index)
{
    script_driver_t* driver = NULL;

    xSemaphoreTake(script_mutex, portMAX_DELAY);

    if (L != NULL) {
        driver = (script_driver_t*)malloc(sizeof(script_driver_t));
        memset((void*)driver, 0, sizeof(script_driver_t));
        l_evse_get_driver(L, index);

        lua_getfield(L, -1, "name");
        if (lua_isstring(L, -1)) {
            driver->name = strdup(lua_tostring(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "description");
        if (lua_isstring(L, -1)) {
            driver->description = strdup(lua_tostring(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "readconfig");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 1, 0) == LUA_OK) {
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -2, "config");

                    l_read_cfg_keys(driver);

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
    }

    xSemaphoreGive(script_mutex);

    return driver;
}

void script_driver_free(script_driver_t* script_driver)
{
    free((void*)script_driver->name);
    free((void*)script_driver->description);
    for (uint8_t i = 0; i < script_driver->cfg_entries_count; i++) {
        script_driver_cfg_meta_entry_t* cfg_entry = &script_driver->cfg_entries[i];
        free((void*)cfg_entry->key);
        free((void*)cfg_entry->name);
        if (cfg_entry->type == SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING) {
            free((void*)cfg_entry->value.string);
        }
    }
    free((void*)script_driver->cfg_entries);
    free((void*)script_driver);
}

void script_driver_cfg_entries_free(script_driver_cfg_entry_t* cfg_entries, uint8_t cfg_entries_count)
{
    for (uint8_t i = 0; i < cfg_entries_count; i++) {
        script_driver_cfg_entry_t* cfg_entry = &cfg_entries[i];
        free((void*)cfg_entry->key);
        if (cfg_entry->type == SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING) {
            free((void*)cfg_entry->value.string);
        }
    }
    free((void*)cfg_entries);
}

// lua args: config meta
void l_json_to_config_table(const script_driver_cfg_entry_t* cfg_entries, uint8_t cfg_entries_count)
{
    lua_newtable(L);

    int count = lua_rawlen(L, -2);
    for (int i = 1; i <= count; i++) {
        lua_rawgeti(L, -2, i);

        lua_getfield(L, -1, "type");
        script_driver_cfg_entry_type_t type = script_str_to_driver_cfg_entry_type(lua_tostring(L, -1));
        lua_pop(L, 1);

        if (type != SCRIPT_DRIVER_CFG_ENTRY_TYPE_NONE) {
            lua_getfield(L, -1, "key");
            const char* key = lua_tostring(L, -1);

            for (uint8_t j = 0; j < cfg_entries_count; j++) {
                const script_driver_cfg_entry_t* cfg_entry = &cfg_entries[j];
                if (strcmp(key, cfg_entry->key) == 0) {
                    if (type == cfg_entry->type) {
                        switch (type) {
                        case SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING:
                            lua_pushstring(L, cfg_entry->value.string);
                            break;
                        case SCRIPT_DRIVER_CFG_ENTRY_TYPE_NUMBER:
                            lua_pushnumber(L, cfg_entry->value.number);
                            break;
                        case SCRIPT_DRIVER_CFG_ENTRY_TYPE_BOOLEAN:
                            lua_pushboolean(L, cfg_entry->value.boolean);
                            break;
                        default:
                            break;
                        }
                    } else {
                        lua_pushnil(L);
                    }
                    lua_setfield(L, -4, key);
                    break;
                }
            }

            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
}

void script_driver_set_config(uint8_t driver_index, const script_driver_cfg_entry_t* cfg_entries, uint8_t cfg_entries_count)
{
    xSemaphoreTake(script_mutex, portMAX_DELAY);

    if (L != NULL) {
        l_evse_get_driver(L, driver_index);

        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "writeconfig");
            if (lua_isfunction(L, -1)) {
                lua_getfield(L, -2, "config");
                l_json_to_config_table(cfg_entries, cfg_entries_count);
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
        }
        lua_pop(L, 1);
    }

    xSemaphoreGive(script_mutex);
}

script_driver_cfg_entry_type_t script_str_to_driver_cfg_entry_type(const char* str)
{
    if (!strcmp(str, "string")) {
        return SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING;
    }
    if (!strcmp(str, "number")) {
        return SCRIPT_DRIVER_CFG_ENTRY_TYPE_NUMBER;
    }
    if (!strcmp(str, "boolean")) {
        return SCRIPT_DRIVER_CFG_ENTRY_TYPE_BOOLEAN;
    }
    return SCRIPT_DRIVER_CFG_ENTRY_TYPE_NONE;
}

const char* script_driver_cfg_entry_type_to_str(script_driver_cfg_entry_type_t type)
{
    switch (type) {
    case SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING:
        return "string";
    case SCRIPT_DRIVER_CFG_ENTRY_TYPE_NUMBER:
        return "number";
    case SCRIPT_DRIVER_CFG_ENTRY_TYPE_BOOLEAN:
        return "boolean";
    default:
        return SCRIPT_DRIVER_CFG_ENTRY_TYPE_NONE;
    }
}