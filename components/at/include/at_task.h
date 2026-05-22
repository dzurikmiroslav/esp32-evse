#ifndef AT_TASK_H_
#define AT_TASK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define AT_TASK_RX_BUFFER_SIZE 256
#define AT_TASK_TX_BUFFER_SIZE 0

/**
 * @brief Start AT task
 *
 * @param file
 * @return TaskHandle_t
 */
TaskHandle_t at_task_start(int fd);

/**
 * @brief Stop AT task
 *
 * @param task
 */
void at_task_stop(TaskHandle_t task);

#endif /* AT_TASK_H_ */
