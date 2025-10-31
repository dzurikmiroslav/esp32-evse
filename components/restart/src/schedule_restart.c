#include "schedule_restart.h"

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void restart_func(void* arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
    vTaskDelete(NULL);
}

void schedule_restart(void)
{
    xTaskCreate(restart_func, "restart", 2 * 1024, NULL, 10, NULL);
}
