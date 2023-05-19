#include <stdio.h>
#include <memory.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "logger.h"
#include "output_buffer.h"

#define LOG_BUFFER_SIZE     6096 //4096
#define MAX_LOG_SIZE        512

static uint8_t log_buffer[LOG_BUFFER_SIZE];

static uint16_t log_count = 0;

static uint8_t* log_append = NULL;

static SemaphoreHandle_t mutex;

static output_buffer_t * buffer = NULL;

EventGroupHandle_t logger_event_group = NULL;

void logger_init(void)
{
    mutex = xSemaphoreCreateMutex();
    logger_event_group = xEventGroupCreate();

    buffer = output_buffer_create(LOG_BUFFER_SIZE);

  //  log_append = log_buffer;
}

uint16_t logger_count(void)
{
    return buffer->count;
    //return log_count;
}

// static void logger_print_len(const char* str, uint16_t len)
// {
//     if (((log_append - log_buffer) + sizeof(uint16_t) + len) >= LOG_BUFFER_SIZE) {
//         //rotate logs
//         uint8_t* pos = log_buffer;
//         uint16_t rotate_count = 0;
//         while ((pos - log_buffer) < LOG_BUFFER_SIZE / 2) {
//             //seek first half
//             uint16_t entry_len;
//             memcpy((void*)&entry_len, (void*)pos, sizeof(uint16_t));
//             pos += entry_len + sizeof(uint16_t);
//             rotate_count++;
//         }

//         memmove((void*)log_buffer, (void*)pos, LOG_BUFFER_SIZE - (pos - log_buffer));
//         log_count -= rotate_count;
//         log_append -= (pos - log_buffer);
//     }

//     memcpy((void*)log_append, (void*)&len, sizeof(uint16_t));
//     log_append += sizeof(uint16_t);

//     memcpy((void*)log_append, (void*)str, len);
//     log_append += len;

//     log_count++;
//    // xEventGroupSetBits(logger_event_group, 0xFF);
// }

void logger_print(const char* str)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    output_buffer_append_str(buffer, str);
    //logger_print_len(str, strlen(str));
    xEventGroupSetBits(logger_event_group, 0xFF);
  
    xSemaphoreGive(mutex);
}

void logger_vprintf(const char* str, va_list l)
{
#ifdef CONFIG_ESP_CONSOLE_UART
    vprintf(str, l);
#endif

    xSemaphoreTake(mutex, portMAX_DELAY);

    static char log[MAX_LOG_SIZE];
    uint16_t len = vsnprintf(log, MAX_LOG_SIZE, str, l);

    //logger_print_len(log, len);
    output_buffer_append_buf(buffer, log, len);
    xEventGroupSetBits(logger_event_group, 0xFF);

    xSemaphoreGive(mutex);
}

bool logger_read(uint16_t* index, char** str, uint16_t* len)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    bool has_next = output_buffer_read(buffer, index, str, len);

    // if (*index > log_count) {
    //     *index = log_count;
    // }

    // bool has_next = false;

    // if (*index < log_count) {
    //     uint8_t* pos = log_buffer;
    //     uint16_t current = 0;

    //     while (current != *index) {
    //         uint16_t entry_len;
    //         memcpy((void*)&entry_len, (void*)pos, sizeof(uint16_t));
    //         pos += entry_len + sizeof(uint16_t);
    //         current++;
    //     }

    //     memcpy((void*)len, (void*)pos, sizeof(uint16_t));
    //     pos += sizeof(uint16_t);
    //     *str = (char*)pos;

    //     (*index)++;

    //     has_next = true;
    // }

    xSemaphoreGive(mutex);

    return has_next;
}