#ifndef SCRIPT_UTILS_H_
#define SCRIPT_UTILS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "be_vm.h"

extern bvm* script_vm;

extern SemaphoreHandle_t script_mutex;

void script_handle_result(int ret);

void script_watchdog_reset(void);

void script_watchdog_disable(void);

void script_output_print(const char *buffer, size_t length);

#endif /* SCRIPT_UTILS_H_ */