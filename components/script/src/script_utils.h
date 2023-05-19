#ifndef SCRIPT_UTILS_H_
#define SCRIPT_UTILS_H_

#include "be_vm.h"

void script_handle_result(bvm* vm, int ret);

void script_watchdog_reset(void);

void script_watchdog_disable(void);

void script_output_print(const char *buffer, size_t length);

#endif /* SCRIPT_UTILS_H_ */