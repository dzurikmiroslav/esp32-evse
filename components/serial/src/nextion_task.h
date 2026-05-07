#ifndef NEXTION_TASK_H_
#define NEXTION_TASK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NEXTION_TASK_RX_BUFFER_SIZE 256
#define NEXTION_TASK_TX_BUFFER_SIZE 0

/**
 * @brief Start Nextion task
 *
 * @param file
 * @return TaskHandle_t
 */
TaskHandle_t nextion_task_start(int fd);

/**
 * @brief Stop Nextion task
 *
 * @param task
 */
void nextion_task_stop(TaskHandle_t task);

void nextion_task_pause(TaskHandle_t task);

void nextion_task_resume(TaskHandle_t task);

int nextion_task_get_fd(TaskHandle_t task);

#endif /* NEXTION_TASK_H_ */
