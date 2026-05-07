#ifndef MODBUS_RTU_TASK_H_
#define MODBUS_RTU_TASK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define MODBUS_RTU_TASK_RX_BUFFER_SIZE 256
#define MODBUS_RTU_TASK_TX_BUFFER_SIZE 0

/**
 * @brief Start Modbus-RTU task
 *
 * @param file
 * @return TaskHandle_t
 */
TaskHandle_t modbus_rtu_task_start(int fd);

/**
 * @brief Stop Modbus-RTU task
 *
 * @param task
 */
void modbus_rtu_task_stop(TaskHandle_t task);

#endif /* MODBUS_RTU_TASK_H_ */
