#include "schedule_restart.h"

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

static void reastart_timer_callback(TimerHandle_t timer)
{
    esp_restart();

    xTimerDelete(timer, 0);
}

void schedule_restart(void)
{
    TimerHandle_t timer = xTimerCreate("restart", pdMS_TO_TICKS(5000), pdFALSE, NULL, reastart_timer_callback);
    xTimerStart(timer, 0);
}
