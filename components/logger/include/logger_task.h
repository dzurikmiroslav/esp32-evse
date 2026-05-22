#ifndef LOGGER_TASK_H_
#define LOGGER_TASK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "improv.h"

/**
 * @brief Logger also implement Improv protocol
 */
#define LOGGER_TASK_RX_BUFFER_SIZE IMPROV_PACKET_SIZE
#define LOGGER_TASK_TX_BUFFER_SIZE 0

/**
 * @brief Start logger task
 *
 * @param file
 * @return TaskHandle_t
 */
TaskHandle_t logger_task_start(int fd);

/**
 * @brief Stop logger task
 *
 * @param task
 */
void logger_task_stop(TaskHandle_t task);

#endif /* LOGGER_TASK_H_ */
