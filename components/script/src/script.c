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

#define START_TIMEOUT           1000
#define SHUTDOWN_TIMEOUT        1000
#define HEARTBEAT_THRESHOLD     5

#define NVS_NAMESPACE           "script"
#define NVS_ENABLED             "enabled"

static const char* TAG = "script";

static nvs_handle nvs;

static TaskHandle_t script_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

static QueueHandle_t start_queue = NULL;

static bvm* vm = NULL;

static int heartbeat_counter;

void script_log_error(bvm* vm, int ret)
{
    if (ret == BE_EXEC_ERROR) {
        ESP_LOGW(TAG, "Error: %s", be_tostring(vm, -1));
        be_pop(vm, 1);
    }
    if (ret == BE_EXCEPTION) {
        ESP_LOGW(TAG, "Exception: '%s' - %s", be_tostring(vm, -2), be_tostring(vm, -1));
        be_pop(vm, 2);
    }
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
    vm = be_vm_new();
    be_set_obs_hook(vm, &obs_hook);
    comp_set_strict(vm);

    int ret = be_loadfile(vm, "/data/main.be");
    if (ret) {
        script_log_error(vm, ret);
    } else {
        script_watchdog_reset();
        ret = be_pcall(vm, 0);
        script_watchdog_disable();
        if (ret) {
            script_log_error(vm, ret);
        }
    }

    bool started = ret == 0;
    xQueueSend(start_queue, (void*)&started, 0);

    while (shutdown_sem == NULL) {
        if (be_top(vm) > 1) {
            ESP_LOGW(TAG, "Berry top: %d", be_top(vm));
        }

        be_getglobal(vm, "evse");
        if (!be_isnil(vm, -1)) {
            be_getmethod(vm, -1, "_process");
            if (!be_isnil(vm, -1)) {
                be_pushvalue(vm, -2);
                ret = be_pcall(vm, 1);
                if (ret) {
                    script_log_error(vm, ret);
                }
                be_pop(vm, 1);
            }
            be_pop(vm, 1);
        }
        be_pop(vm, 1);

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    be_vm_delete(vm);
    vm = NULL;

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
            ESP_LOGE(TAG, "Server task stop timeout, will be force stoped");
            vTaskDelete(script_task);
            if (vm != NULL) {
                be_vm_delete(vm);
                vm = NULL;
            }
        }

        vSemaphoreDelete(shutdown_sem);
        shutdown_sem = NULL;
        script_task = NULL;
    }
}

static esp_err_t script_start(void)
{
    if (!script_task) {
        ESP_LOGI(TAG, "Starting script");
        xTaskCreate(script_task_func, "script_task", 4 * 1024, NULL, 5, &script_task);

        bool started = false;
        if (xQueueReceive(start_queue, (void*)&started, pdMS_TO_TICKS(START_TIMEOUT))) {
            return started ? ESP_OK : ESP_ERR_INVALID_STATE;
        }
        return ESP_ERR_TIMEOUT;
    }
    return ESP_ERR_INVALID_STATE;
}

void script_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    start_queue = xQueueCreate(1, sizeof(bool));

    if (script_is_enabled()) {
        script_start();
    }
}

esp_err_t script_reload(void)
{
    if (script_is_enabled()) {
        script_stop();
        return script_start();
    }
    return ESP_OK;
}

esp_err_t script_set_enabled(bool enabled)
{
    nvs_set_u8(nvs, NVS_ENABLED, enabled);

    nvs_commit(nvs);

    if (enabled) {
        return script_start();
    } else {
        script_stop();
        return ESP_OK;
    }
}

bool script_is_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}
