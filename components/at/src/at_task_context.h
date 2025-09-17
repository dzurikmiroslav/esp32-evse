#ifndef AT_TASK_CONTEXT_H_
#define AT_TASK_CONTEXT_H_

#include "at.h"

bool at_task_context_subscribe(at_task_context_t* context, const char* command_name, uint32_t period);

bool at_task_context_unsubscribe(at_task_context_t* context, const char* command_name);

#endif /* AT_TASK_CONTEXT_H_ */