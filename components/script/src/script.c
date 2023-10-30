#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs.h"
#include "be_vm.h"
#include "be_module.h"
#include "be_debug.h"

#include "script.h"
#include "script_utils.h"
#include "output_buffer.h"

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

bvm* script_vm = NULL;

SemaphoreHandle_t script_mutex = NULL;

static int heartbeat_counter = -1;

static output_buffer_t* output_buffer = NULL;

static SemaphoreHandle_t output_mutex;

void script_handle_result(bvm* vm, int res)
{
    switch (res) {
    case BE_EXCEPTION:
        be_dumpexcept(vm);
        break;
    case BE_EXIT:
        be_toindex(vm, -1);
        break;
    case BE_IO_ERROR:
        be_writestring("error: ");
        be_writestring(be_tostring(vm, -1));
        be_writenewline();
        break;
    case BE_MALLOC_FAIL:
        be_writestring("error: memory allocation failed.\n");
        break;
    default:
        break;
    }

    // if (ret == BE_EXEC_ERROR) {
    //     //ESP_LOGW(TAG, "Error: %s", be_tostring(vm, -1));
    //     output_buffer_append_str(output_buffer, "Error: ");
    //     output_buffer_append_str(output_buffer, be_tostring(vm, -1));
    //     output_buffer_append_str(output_buffer, "\n");
    //     be_pop(vm, 1);
    // }
    // if (ret == BE_EXCEPTION) {
    //     //ESP_LOGW(TAG, "Exception: '%s' - %s", be_tostring(vm, -2), be_tostring(vm, -1));
    //     output_buffer_append_str(output_buffer, "Exception: '");
    //     output_buffer_append_str(output_buffer, be_tostring(vm, -2));
    //     output_buffer_append_str(output_buffer, "' - ");
    //     output_buffer_append_str(output_buffer, be_tostring(vm, -2));
    //     output_buffer_append_str(output_buffer, "\n");
    //     be_pop(vm, 2);
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

static void obs_hook(bvm* vm, int event, ...)
{
    switch (event) {
    case BE_OBS_VM_HEARTBEAT:
        if (heartbeat_counter >= 0) {
            if (heartbeat_counter++ > HEARTBEAT_THRESHOLD) {
                be_raise(vm, "timeout_error", "code running for too long");
            }
        }
        break;
    default:
        break;
    }
}

static void script_task_func(void* param)
{
    xSemaphoreTake(output_mutex, portMAX_DELAY);
    output_buffer_append_str(output_buffer, "berry " BERRY_VERSION "\n");
    xSemaphoreGive(output_mutex);

    script_vm = be_vm_new();
    be_set_obs_hook(script_vm, &obs_hook);
    comp_set_strict(script_vm);

    xSemaphoreTake(output_mutex, portMAX_DELAY);
    output_buffer_append_str(output_buffer, "loading file '/data/main.be'...\n");
    xSemaphoreGive(output_mutex);

    int ret = be_loadfile(script_vm, "/data/main.be");
    if (ret == 0) {
        script_watchdog_reset();
        ret = be_pcall(script_vm, 0);
        script_watchdog_disable();
    }

    script_handle_result(script_vm, ret);

    while (shutdown_sem == NULL) {
        if (be_top(script_vm) > 1) {
            ESP_LOGW(TAG, "Berry top: %d", be_top(script_vm));
        }

        xSemaphoreTake(script_mutex, portMAX_DELAY);

        be_getglobal(script_vm, "evse");
        if (!be_isnil(script_vm, -1)) {
            be_getmethod(script_vm, -1, "_process");
            if (!be_isnil(script_vm, -1)) {
                be_pushvalue(script_vm, -2);
                ret = be_pcall(script_vm, 1);
                script_handle_result(script_vm, ret);
                be_pop(script_vm, 1);
            }
            be_pop(script_vm, 1);
        }
        be_pop(script_vm, 1);

        xSemaphoreGive(script_mutex);

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    be_vm_delete(script_vm);
    script_vm = NULL;

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
            if (script_vm != NULL) {
                be_vm_delete(script_vm);
                script_vm = NULL;
            }
        }

        vSemaphoreDelete(shutdown_sem);
        shutdown_sem = NULL;
        script_task = NULL;

        xSemaphoreTake(output_mutex, portMAX_DELAY);
        output_buffer_delete(output_buffer);
        output_buffer = NULL;
        xSemaphoreGive(output_mutex);
    }
}

void script_start(void)
{
    if (!script_task) {
        output_buffer = output_buffer_create(OUTPUT_BUFFER_SIZE);

        ESP_LOGI(TAG, "Starting script");
        xTaskCreate(script_task_func, "script_task", 4 * 1024, NULL, 5, &script_task);
    }
}

void script_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    script_mutex = xSemaphoreCreateMutex();
    output_mutex = xSemaphoreCreateMutex();

    if (script_is_enabled()) {
        script_start();
    }
}

void script_output_print(const char* buffer, size_t length)
{
    xSemaphoreTake(output_mutex, portMAX_DELAY);

    output_buffer_append_buf(output_buffer, buffer, length);

    xSemaphoreGive(output_mutex);
}

uint16_t script_output_count(void)
{
    return output_buffer != NULL ? output_buffer->count : 0;
}

bool script_output_read(uint16_t* index, char** str, uint16_t* len)
{
    xSemaphoreTake(output_mutex, portMAX_DELAY);

    bool has_next = false;
    if (output_buffer != NULL) {
        has_next = output_buffer_read(output_buffer, index, str, len);
    }

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
