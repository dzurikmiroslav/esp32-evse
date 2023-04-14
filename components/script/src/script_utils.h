#ifndef __SCRIPT_LOG___
#define __SCRIPT_LOG___

#include "be_vm.h"

void script_log_error(bvm* vm, int ret);

void script_watchdog_reset(void);

void script_watchdog_disable(void);

#endif // __SCRIPT_LOG___