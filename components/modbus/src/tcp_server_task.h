#ifndef TCP_SERVER_TASK_H
#define TCP_SERVER_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief Start Modbus-TCP task
 *
 * @return TaskHandle_t
 */
TaskHandle_t tcp_server_task_start(void);

/**
 * @brief Stop Modbus-TCP task
 *
 * @param task
 */
void tcp_server_task_stop(TaskHandle_t task);

#endif /* TCP_SERVER_TASK_H */