#include "logger_task.h"

#include <unistd.h>

#include "improv.h"
#include "logger.h"

static void logger_task_func(void* param)
{
    int fd = (int)(intptr_t)param;

    char* str;
    uint16_t str_len;
    uint16_t index = 0;
    while (true) {
        while (logger_log_bugger_read(&index, &str, &str_len)) {
            write(fd, str, str_len);
            fsync(fd);
        }

        improv_handle(fd);  // max delay 250ms
    }
}

TaskHandle_t logger_task_start(int fd)
{
    TaskHandle_t handle = NULL;
    xTaskCreate(logger_task_func, "logger", 3 * 1024, (void*)fd, 5, &handle);
    return handle;
}

void logger_task_stop(TaskHandle_t task)
{
    vTaskSuspend(task);
    vTaskDelete(task);
}