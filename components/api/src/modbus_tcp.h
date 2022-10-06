#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

extern QueueHandle_t modbus_tcp_req_queue;

extern QueueHandle_t modbus_tcp_res_queue;

void modbus_tcp_init(void);

void modbus_tcp_listen(void);

void modbus_tcp_stop(void);

#endif /* MODBUS_TCP_H */