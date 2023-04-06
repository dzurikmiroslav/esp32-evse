#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"
#include "berry.h"

#include "script.h"

#define SHUTDOWN_TIMEOUT        1000

#define NVS_NAMESPACE           "script"
#define NVS_ENABLED             "enabled"

static const char* TAG = "script";

static nvs_handle nvs;

static TaskHandle_t script_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

static bvm* vm = NULL;

static void script_task_func(void* param)
{
    vm = be_vm_new();

    int ret = be_loadfile(vm, "/data/main.be");

    if (!ret) {
        ret = be_pcall(vm, 0);
        if (ret) {
            ESP_LOGW(TAG, "Berry call ex: %d", be_getexcept(vm, ret));
        }
    } else {
        ESP_LOGW(TAG, "Berry load ex: %d", be_getexcept(vm, ret));
    }

    while (shutdown_sem == NULL) {
        //ESP_LOGW(TAG, "Berry top: %d", be_top(vm));

        be_getglobal(vm, "timer");
        if (!be_isnil(vm, -1)) {
            be_getmethod(vm, -1, "process");
            if (!be_isnil(vm, -1)) {
                be_pushvalue(vm, -2);
                ret = be_pcall(vm, 1);
                if (ret) {
                    be_dumpexcept(vm);
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

void script_repl_run(char* cmd)
{
    if (vm == NULL) {
        ESP_LOGW(TAG, "Berry not started");
        return;
    }

    size_t cmd_len = strlen(cmd);
    size_t cmd2_len = cmd_len + 12;
    char* cmd2 = (char*)malloc(cmd2_len);
    snprintf(cmd2, cmd2_len, "return (%s)", cmd);

    int ret = be_loadbuffer(vm, "input", cmd2, strlen(cmd2));
    if (be_getexcept(vm, ret) == BE_SYNTAX_ERROR) {
        be_pop(vm, 2);
        // if fails, try the direct command
        ret = be_loadbuffer(vm, "input", cmd, cmd_len);
    }

    if (ret == 0) {
        ret = be_pcall(vm, 0);
        if (ret == 0) {
            if (!be_isnil(vm, 1)) {
                const char* ret_val = be_tostring(vm, 1);
                ESP_LOGI(TAG, "Berry return: %s", ret_val);
            }
            be_pop(vm, 1);
        } else {
            ESP_LOGW(TAG, "Berry call ex: %d", be_getexcept(vm, ret));
        }
    }

    if (cmd2 != NULL) {
        free(cmd2);
    }
}

void script_stop(void)
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

static void script_start(void)
{
    if (!script_task) {
        ESP_LOGI(TAG, "Starting script");
        xTaskCreate(script_task_func, "script_task", 4 * 1024, NULL, 5, &script_task);
    }
}

void script_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    if (script_is_enabled()) {
        script_start();
    }
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
