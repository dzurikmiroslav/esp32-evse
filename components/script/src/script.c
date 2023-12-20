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

//bvm* script_vm = NULL;

static lua_State* L = NULL;

SemaphoreHandle_t script_mutex = NULL;

static int heartbeat_counter = -1;

static output_buffer_t* output_buffer = NULL;

static SemaphoreHandle_t output_mutex;


void script_handle_result(int res)
{
    // switch (res) {
    // case BE_EXCEPTION:
    //     be_dumpexcept(script_vm);
    //     break;
    // case BE_EXIT:
    //     be_toindex(script_vm, -1);
    //     break;
    // case BE_IO_ERROR:
    //     be_writestring("error: ");
    //     be_writestring(be_tostring(script_vm, -1));
    //     be_writenewline();
    //     break;
    // case BE_MALLOC_FAIL:
    //     be_writestring("error: memory allocation failed.\n");
    //     break;
    // default:
    //     break;
    // }
}

void script_watchdog_reset(void)
{
    heartbeat_counter = 0;
}

void script_watchdog_disable(void)
{
    heartbeat_counter = -1;
}

//static void obs_hook(bvm* vm, int event, ...)
//{
    // switch (event) {
    // case BE_OBS_VM_HEARTBEAT:
    //     if (heartbeat_counter >= 0) {
    //         if (heartbeat_counter++ > HEARTBEAT_THRESHOLD) {
    //             be_raise(vm, "timeout_error", "code running for too long");
    //         }
    //     }
    //     break;
    // default:
    //     break;
    // }
//}

#define LOADING_MSG     "loading file '/data/init.lua'..."

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

    lua_writestring(LOADING_MSG, strlen(LOADING_MSG));
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

        lua_getglobal(L, "evse");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "__process");
            if (lua_isfunction(L, -1)) {
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    lua_writestring(err, strlen(err));
                    lua_writeline();
                }
            }
        }
        lua_pop(L, lua_gettop(L));

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    lua_close(L);
    L = NULL;

    // script_vm = be_vm_new();
    // be_set_obs_hook(script_vm, &obs_hook);
    // comp_set_strict(script_vm);

    // xSemaphoreTake(output_mutex, portMAX_DELAY);
    // output_buffer_append_str(output_buffer, "loading file '/data/main.be'...\n");
    // xSemaphoreGive(output_mutex);

    // int ret = be_loadfile(script_vm, "/data/main.be");
    // if (ret == 0) {
    //     script_watchdog_reset();
    //     ret = be_pcall(script_vm, 0);
    //     script_watchdog_disable();
    // }

    // script_handle_result(ret);

    // while (true) {
    //     xSemaphoreTake(script_mutex, portMAX_DELAY);
    //     if (shutdown_sem != NULL) {
    //         xSemaphoreGive(script_mutex);
    //         break;
    //     }

    //     if (be_top(script_vm) > 1) {
    //         ESP_LOGW(TAG, "Berry top: %d", be_top(script_vm));
    //     }

    //     be_getglobal(script_vm, "evse");
    //     if (!be_isnil(script_vm, -1)) {
    //         be_getmethod(script_vm, -1, "_process");
    //         if (!be_isnil(script_vm, -1)) {
    //             be_pushvalue(script_vm, -2);
    //             ret = be_pcall(script_vm, 1);
    //             script_handle_result(ret);
    //             be_pop(script_vm, 1);
    //         }
    //         be_pop(script_vm, 1);
    //     }
    //     be_pop(script_vm, 1);

    //     xSemaphoreGive(script_mutex);

    //     vTaskDelay(pdMS_TO_TICKS(50));
    // }

    // be_vm_delete(script_vm);
    // script_vm = NULL;

    if (shutdown_sem) {
        xSemaphoreGive(shutdown_sem);
    }
    vTaskDelete(NULL);
}

void scrip_lock(lua_State* L)
{
    xSemaphoreTake(script_mutex, portMAX_DELAY);
}

void scrip_unlock(lua_State* L)
{
    xSemaphoreGive(script_mutex);
}

static void script_stop(void)
{
    if (script_task) {
        ESP_LOGI(TAG, "Stopping script");
        // xSemaphoreTake(script_mutex, portMAX_DELAY);
        shutdown_sem = xSemaphoreCreateBinary();
        // xSemaphoreGive(script_mutex);

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

    script_mutex = xSemaphoreCreateMutex();
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
