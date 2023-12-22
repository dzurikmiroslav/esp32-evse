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
        l_evse_process(L);

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
