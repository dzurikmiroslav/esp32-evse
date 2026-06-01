#include "logger_file.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "logger.h"

#define LOG_FILE          "/usr/system.log"
#define LOG_FILE_PREVIOUS "/usr/system.log.previous"
#define LOG_FILE_MAX_SIZE (64 * 1024)  // rotate before a single file grows past this
#define FLUSH_PERIOD_MS   1000         // batch flash writes; up to this much tail may be lost on a hard crash

static FILE* file = NULL;
static size_t file_size = 0;

static void rotate_log(void)
{
    if (file != NULL) {
        fclose(file);
        file = NULL;
    }

    // Keep the just-finished log as the "previous" one (replacing any older
    // previous), then start a fresh current log. rename() fails harmlessly when
    // LOG_FILE does not exist yet (first ever boot).
    rename(LOG_FILE, LOG_FILE_PREVIOUS);

    file = fopen(LOG_FILE, "w");
    file_size = 0;
}

static void logger_file_task([[maybe_unused]] void* param)
{
    uint16_t index = 0;
    char* str;
    uint16_t len;

    while (true) {
        bool dirty = false;

        while (logger_read(&index, &str, &len)) {
            if (file == NULL) {
                continue;  // still drain the ring so we don't replay it later
            }

            // Build one uniform timestamp prefix: local "YYYY-MM-DD HH:MM:SS" once
            // the clock is set (after SNTP sync or a manual set), otherwise an
            // uptime "[ssss.mmm]" - so with NTP disabled / not yet synced we never
            // emit a misleading 1970/01:00 wall-clock. The timezone follows the
            // device TZ set by the scheduler. Taken at drain time (~1s batches),
            // so it may trail the actual log moment slightly.
            char ts[32];
            size_t ts_len;
            time_t now_s = time(NULL);
            struct tm tm_now;
            localtime_r(&now_s, &tm_now);
            if (tm_now.tm_year + 1900 >= 2020) {
                ts_len = strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S ", &tm_now);
            } else {
                uint64_t up_ms = (uint64_t)pdTICKS_TO_MS(xTaskGetTickCount());
                ts_len = snprintf(ts, sizeof(ts), "[%6llu.%03llu] ", (unsigned long long)(up_ms / 1000),
                    (unsigned long long)(up_ms % 1000));
            }
            if (ts_len > sizeof(ts)) {
                ts_len = sizeof(ts);
            }
            fwrite(ts, 1, ts_len, file);
            file_size += ts_len;

            // Strip esp_log's own "(<timestamp>)" token so the line isn't
            // double-timestamped. Format is "<LEVEL> (<ts>) <tag>: <msg>"; keep
            // "<LEVEL> <tag>: <msg>". Any line not matching is written as-is.
            const char* open = memchr(str, '(', len);
            const char* close = open != NULL ? memchr(open, ')', len - (open - str)) : NULL;
            if (open != NULL && close != NULL) {
                size_t head_len = open - str;  // "<LEVEL> "
                while (head_len > 0 && str[head_len - 1] == ' ') {
                    head_len--;  // -> "<LEVEL>"
                }
                const char* rest = close + 1;  // " <tag>: <msg>\n"
                size_t rest_len = len - (rest - str);
                if (rest_len > 0 && rest[0] == ' ') {
                    rest++;
                    rest_len--;
                }
                fwrite(str, 1, head_len, file);
                fwrite(" ", 1, 1, file);
                fwrite(rest, 1, rest_len, file);
                file_size += head_len + 1 + rest_len;
            } else {
                fwrite(str, 1, len, file);
                file_size += len;
            }

            dirty = true;

            if (file_size >= LOG_FILE_MAX_SIZE) {
                fflush(file);
                fsync(fileno(file));
                rotate_log();
                dirty = false;
            }
        }

        if (dirty && file != NULL) {
            fflush(file);
            fsync(fileno(file));
        }

        vTaskDelay(pdMS_TO_TICKS(FLUSH_PERIOD_MS));
    }
}

void logger_file_init(void)
{
    rotate_log();

    xTaskCreate(logger_file_task, "logger_file", 4 * 1024, NULL, 5, NULL);
}
