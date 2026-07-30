#include "logger.h"

#include <esp_log.h>
#include <esp_private/panic_internal.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>

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

// Build the per-line prefix: local "YYYY-MM-DD HH:MM:SS " once the clock is set
// (after SNTP sync / manual set), otherwise an uptime "[ssss.mmm] " - so with NTP
// disabled or not yet synced we never emit a misleading 1970/01:00 wall-clock.
// Timezone follows the device TZ set by the scheduler.
static size_t format_timestamp(char* dst, size_t dst_size)
{
    time_t now_s = time(NULL);
    struct tm tm_now;
    localtime_r(&now_s, &tm_now);

    size_t n;
    if (tm_now.tm_year + 1900 >= 2020) {
        n = strftime(dst, dst_size, "%Y-%m-%d %H:%M:%S ", &tm_now);
    } else {
        uint64_t up_ms = (uint64_t)pdTICKS_TO_MS(xTaskGetTickCount());
        n = snprintf(dst, dst_size, "[%6llu.%03llu] ", (unsigned long long)(up_ms / 1000), (unsigned long long)(up_ms % 1000));
    }
    return MIN(n, dst_size);
}

// Copy a line-start fragment with esp_log's own "(<timestamp>)" token removed, so
// it is not double-timestamped. Format is "<LEVEL> (<ts>) <tag>: <msg>"; result is
// "<LEVEL> <tag>: <msg>". A fragment not matching that shape is copied verbatim.
static size_t strip_esp_log_timestamp(const char* log, size_t len, char* dst, size_t dst_size)
{
    const char* open = memchr(log, '(', len);
    const char* close = open != NULL ? memchr(open, ')', len - (open - log)) : NULL;
    if (open == NULL || close == NULL) {
        size_t n = MIN(len, dst_size);
        memcpy(dst, log, n);
        return n;
    }

    size_t head_len = open - log;  // "<LEVEL> "
    while (head_len > 0 && log[head_len - 1] == ' ') {
        head_len--;  // -> "<LEVEL>"
    }

    const char* rest = close + 1;  // " <tag>: <msg>\n"
    size_t rest_len = len - (rest - log);
    if (rest_len > 0 && rest[0] == ' ') {
        rest++;
        rest_len--;
    }

    size_t pos = MIN(head_len, dst_size);
    memcpy(dst, log, pos);
    if (pos < dst_size) {
        dst[pos++] = ' ';
    }
    size_t n = MIN(rest_len, dst_size - pos);
    memcpy(dst + pos, rest, n);
    return pos + n;
}

static int logger_vprintf(const char* str, va_list l)
{
#ifdef CONFIG_ESP_CONSOLE_UART
    vprintf(str, l);
#endif /* CONFIG_ESP_CONSOLE_UART */

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    static char log[LOG_LINE_SIZE];
    int len = vsnprintf(log, LOG_LINE_SIZE, str, l);
    size_t safe_len = len < 0 ? 0 : MIN((size_t)len, (size_t)(LOG_LINE_SIZE - 1));

    // A single log line can reach the ring as several fragments (the WiFi/ROM
    // logger emits "<LEVEL> (<ts>) <tag>:", the message, and "\n" separately).
    // Prefix our timestamp only at line starts and strip esp_log's own token, so
    // the ring holds clean, singly-timestamped lines for both /log and the file.
    static bool at_line_start = true;
    if (at_line_start && safe_len > 0) {
        static char out[LOG_LINE_SIZE + 32];
        size_t out_len = format_timestamp(out, sizeof(out));
        out_len += strip_esp_log_timestamp(log, safe_len, out + out_len, sizeof(out) - out_len);
        output_buffer_append_buf(s_log_buffer, out, out_len);
    } else {
        output_buffer_append_buf(s_log_buffer, log, safe_len);
    }

    if (safe_len > 0) {
        at_line_start = (log[safe_len - 1] == '\n');
    }

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
