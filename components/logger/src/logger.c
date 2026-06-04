#include "logger.h"

#include <esp_log.h>
#include <esp_private/panic_internal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory.h>
#include <stdio.h>
#include <sys/param.h>

#include "sdkconfig.h"

#include "output_buffer.h"

#define LOG_BUFFER_SIZE 6096  // 4096
#define LOG_LINE_SIZE   512
#define PANIC_MAGIC     0xDEADC0DE
#define PANIC_LOG_SIZE  2048

static SemaphoreHandle_t s_mutex;

static uint8_t s_log_buffer_data[LOG_BUFFER_SIZE];

static output_buffer_t* s_log_buffer;

typedef struct {
    uint32_t magic;
    char log[PANIC_LOG_SIZE];
    uint16_t log_len;
    time_t time;
} panic_t;

static RTC_NOINIT_ATTR panic_t s_panic;

static int logger_vprintf(const char* str, va_list l)
{
#ifdef CONFIG_ESP_CONSOLE_UART
    vprintf(str, l);
#endif /* CONFIG_ESP_CONSOLE_UART */

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    static char log[LOG_LINE_SIZE];
    int len = vsnprintf(log, LOG_LINE_SIZE, str, l);

    output_buffer_append_buf(s_log_buffer, log, len);

    xSemaphoreGive(s_mutex);

    return len;
}

void logger_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    s_log_buffer = output_buffer_create_static(LOG_BUFFER_SIZE, s_log_buffer_data);

    esp_log_set_vprintf(logger_vprintf);

    // Set panic time, after reboot
    if (s_panic.magic == PANIC_MAGIC && !s_panic.time) s_panic.time = time(NULL);
}

uint16_t logger_log_buffer_get_count(void)
{
    return s_log_buffer->count;
}

bool logger_log_bugger_read(uint16_t* index, char** str, uint16_t* len)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    bool has_next = output_buffer_read(s_log_buffer, index, str, len);

    xSemaphoreGive(s_mutex);

    return has_next;
}

time_t logger_get_panic_time(void)
{
    return s_panic.magic == PANIC_MAGIC ? s_panic.time : 0;
}

const char* logger_get_panic_log(void)
{
    return s_panic.magic == PANIC_MAGIC ? s_panic.log : NULL;
}

void logger_clear_panic(void)
{
    s_panic.magic = 0;
    s_panic.log_len = 0;
    s_panic.time = 0;
}

#if !CONFIG_ESP_SYSTEM_PANIC_SILENT_REBOOT
void IRAM_ATTR panic_print_char(const char c)
{
    if (s_panic.log_len < PANIC_LOG_SIZE - 1) {
        s_panic.log[s_panic.log_len++] = c;
        s_panic.log[s_panic.log_len] = '\0';
    }
}
#endif /* CONFIG_ESP_SYSTEM_PANIC_SILENT_REBOOT */

extern void __real_esp_panic_handler(panic_info_t* info);

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t* info)
{
    s_panic.magic = PANIC_MAGIC;
    s_panic.log_len = 0;
    s_panic.time = 0;

    __real_esp_panic_handler(info);
}
