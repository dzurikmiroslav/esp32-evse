#include "script.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>
#include <unistd.h>

#include "component_params.h"
#include "l_aux_lib.h"
#include "l_board_config_lib.h"
#include "l_component.h"
#include "l_evse_lib.h"
#include "l_json_lib.h"
#include "l_mqtt_lib.h"
#include "l_serial_lib.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "output_buffer.h"
#include "script.h"
#include "script_utils.h"
#include "script_watchdog.h"

#define SHUTDOWN_TIMEOUT   1000
#define OUTPUT_BUFFER_SIZE 4096

#define NVS_NAMESPACE   "script"
#define NVS_ENABLED     "enabled"
#define NVS_AUTO_RELOAD "auto_reload"

SemaphoreHandle_t script_mutex = NULL;

static const char* TAG = "script";

static nvs_handle nvs;

static TaskHandle_t script_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

static lua_State* L = NULL;

static output_buffer_t* output_buffer = NULL;

static SemaphoreHandle_t output_mutex;

static bool auto_reload = false;

static void script_task_func(void* param)
{
    lua_writestring(LUA_COPYRIGHT, strlen(LUA_COPYRIGHT));
    lua_writeline();

    L = luaL_newstate();

    luaL_openlibs(L);

    l_component_register(L);

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

    luaL_requiref(L, "serial", luaopen_serial, 0);
    lua_pop(L, 1);

    lua_gc(L, LUA_GCSETPAUSE, 110);
    lua_gc(L, LUA_GCSETSTEPMUL, 200);

    script_watchdog_init(L);

    char* init_file = NULL;
    if (access("/storage/lua/init.luac", F_OK) == 0) {
        init_file = "/storage/lua/init.luac";
    } else if (access("/storage/lua/init.lua", F_OK) == 0) {
        init_file = "/storage/lua/init.lua";
    }

    if (init_file) {
        const char *loading_msg_1 =  "loading file '";
        const char *loading_msg_2 =  "loading file '";
        lua_writestring(loading_msg_1, strlen(loading_msg_1));
        lua_writestring(init_file, strlen(init_file));
        lua_writestring(loading_msg_2, strlen(loading_msg_2)); 
        lua_writeline();

        if (luaL_dofile(L, init_file) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            lua_writestring(err, strlen(err));
            lua_writeline();
            lua_pop(L, 1);
        }
    }

    while (true) {
        if (shutdown_sem != NULL) {
            break;
        }
        xSemaphoreTake(script_mutex, portMAX_DELAY);
        script_watchdog_reset();
        l_component_resume(L);
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

static bool path_has_suffix(const char* path, const char* suffix)
{
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);

    return (path_len > suffix_len) && strcmp(path + (path_len - suffix_len), suffix) == 0;
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

    uint8_t u8;
    if (nvs_get_u8(nvs, NVS_AUTO_RELOAD, &u8) == ESP_OK) {
        auto_reload = u8;
    }

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

void script_set_auto_reload(bool _auto_reload)
{
    auto_reload = _auto_reload;

    nvs_set_u8(nvs, NVS_AUTO_RELOAD, auto_reload);
    nvs_commit(nvs);
}

bool script_is_auto_reload(void)
{
    return auto_reload;
}

void script_file_changed(const char* path)
{
    if (auto_reload && (path_has_suffix(path, ".lua") || path_has_suffix(path, ".luac"))) {
        script_reload();
    }
}

script_component_param_type_t script_str_to_component_param_type(const char* str)
{
    if (!strcmp(str, "string")) {
        return SCRIPT_COMPONENT_PARAM_TYPE_STRING;
    }
    if (!strcmp(str, "number")) {
        return SCRIPT_COMPONENT_PARAM_TYPE_NUMBER;
    }
    if (!strcmp(str, "boolean")) {
        return SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN;
    }
    return SCRIPT_COMPONENT_PARAM_TYPE_NONE;
}

const char* script_component_param_type_to_str(script_component_param_type_t type)
{
    switch (type) {
    case SCRIPT_COMPONENT_PARAM_TYPE_STRING:
        return "string";
    case SCRIPT_COMPONENT_PARAM_TYPE_NUMBER:
        return "number";
    case SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN:
        return "boolean";
    default:
        return SCRIPT_COMPONENT_PARAM_TYPE_NONE;
    }
}

script_component_list_t* script_get_components(void)
{
    script_component_list_t* list = NULL;

    xSemaphoreTake(script_mutex, portMAX_DELAY);
    if (L) {
        list = l_component_get_components(L);
    }
    xSemaphoreGive(script_mutex);

    return list;
}

void script_components_free(script_component_list_t* list)
{
    while (!SLIST_EMPTY(list)) {
        script_component_entry_t* item = SLIST_FIRST(list);

        SLIST_REMOVE_HEAD(list, entries);

        if (item->id) free((void*)item->id);
        if (item->name) free((void*)item->name);
        if (item->description) free((void*)item->description);
        free((void*)item);
    }

    free((void*)list);
}

script_component_param_list_t* script_get_component_params(const char* id)
{
    script_component_param_list_t* list = NULL;

    xSemaphoreTake(script_mutex, portMAX_DELAY);
    if (L) {
        list = l_component_get_component_params(L, id);
    }
    xSemaphoreGive(script_mutex);

    return list;
}

void script_set_component_params(const char* id, script_component_param_list_t* list)
{
    // persist parameters
    component_param_list_t* raw_list = (component_param_list_t*)malloc(sizeof(component_param_list_t));
    SLIST_INIT(raw_list);

    script_component_param_entry_t* entry;
    SLIST_FOREACH (entry, list, entries) {
        component_param_entry_t* raw_entry = (component_param_entry_t*)malloc(sizeof(component_param_entry_t));
        raw_entry->key = strdup(entry->key);
        switch (entry->type) {
        case SCRIPT_COMPONENT_PARAM_TYPE_STRING:
            raw_entry->value = entry->value.string ? strdup(entry->value.string) : NULL;
            break;
        case SCRIPT_COMPONENT_PARAM_TYPE_NUMBER:
            raw_entry->value = (char*)malloc(32 * sizeof(char));
            sprintf(raw_entry->value, "%f", entry->value.number);
            break;
        case SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN:
            raw_entry->value = strdup(entry->value.boolean ? "true" : "false");
            break;
        default:
            raw_entry->value = NULL;
        }
        SLIST_INSERT_HEAD(raw_list, raw_entry, entries);
    }

    component_params_write(id, raw_list);
    component_params_free(raw_list);

    // restart component
    xSemaphoreTake(script_mutex, portMAX_DELAY);
    if (L) {
        l_component_restart(L, id);
    }
    xSemaphoreGive(script_mutex);
}

void script_component_params_free(script_component_param_list_t* list)
{
    while (!SLIST_EMPTY(list)) {
        script_component_param_entry_t* item = SLIST_FIRST(list);

        SLIST_REMOVE_HEAD(list, entries);

        if (item->key) free((void*)item->key);
        if (item->name) free((void*)item->name);
        if (item->type == SCRIPT_COMPONENT_PARAM_TYPE_STRING && item->value.string) free((void*)item->value.string);

        free((void*)item);
    }

    free((void*)list);
}