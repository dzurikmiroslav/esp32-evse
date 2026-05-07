#ifndef SERIAL_MODE_H_
#define SERIAL_MODE_H_

#define SERIAL_MODE_AT_NAME      "at"
#define SERIAL_MODE_LOG_NAME     "log"
#define SERIAL_MODE_NEXTION_NAME "nextion"
#define SERIAL_MODE_MODBUS_NAME  "modbus"
#define SERIAL_MODE_SCRIPT_NAME  "script"

#define SEIAL_MODE_NAME_SIZE 16

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "serial.h"

typedef struct {
    const char* name;
    TaskHandle_t (*start)(int fd);
    void (*stop)(TaskHandle_t task);
    int rx_buffer_size;
    int tx_buffer_size;
} serial_mode_conf_t;

extern serial_mode_conf_t serial_mode_configs[];

typedef struct {
    int fd;
    TaskHandle_t task;
    serial_mode_conf_t* mode;
} serial_task_t;

extern serial_task_t serial_tasks[SERIAL_ID_MAX];

int serial_port_find(const char* mode_name);

int serial_fd_find(const char* mode_name);

#endif /* SERIAL_MODE_H_ */