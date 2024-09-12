#include "logger.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory.h>
#include <stdio.h>
#include <sys/param.h>

#include "output_buffer.h"

#define LOG_BUFFER_SIZE 6096  // 4096
#define MAX_LOG_SIZE    512

static SemaphoreHandle_t mutex;

static output_buffer_t* buffer = NULL;

EventGroupHandle_t logger_event_group = NULL;

void logger_init(void)
{
    mutex = xSemaphoreCreateMutex();
    logger_event_group = xEventGroupCreate();

    buffer = output_buffer_create(LOG_BUFFER_SIZE);
}

uint16_t logger_count(void)
{
    return buffer->count;
}

void logger_print(const char* str)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    output_buffer_append_str(buffer, str);
    xEventGroupSetBits(logger_event_group, 0xFF);

    xSemaphoreGive(mutex);
}

int logger_vprintf(const char* str, va_list l)
{
#ifdef CONFIG_ESP_CONSOLE_UART
    vprintf(str, l);
#endif

    xSemaphoreTake(mutex, portMAX_DELAY);

    static char log[MAX_LOG_SIZE];
    int len = vsnprintf(log, MAX_LOG_SIZE, str, l);

    output_buffer_append_buf(buffer, log, len);
    xEventGroupSetBits(logger_event_group, 0xFF);

    xSemaphoreGive(mutex);

    return len;
}

bool logger_read(uint16_t* index, char** str, uint16_t* len)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    bool has_next = output_buffer_read(buffer, index, str, len);

    xSemaphoreGive(mutex);

    return has_next;
}