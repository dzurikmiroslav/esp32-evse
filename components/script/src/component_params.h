#ifndef COMPONENT_PARAMS_H_
#define COMPONENT_PARAMS_H_

#include <sys/queue.h>

typedef struct component_param_s {
    char* key;
    char* value;

    SLIST_ENTRY(component_param_s) entries;
} component_param_entry_t;

typedef SLIST_HEAD(component_param_list_s, component_param_s) component_param_list_t;

component_param_list_t* component_params_read(const char* component);

void component_params_write(const char* component, component_param_list_t*);

void component_params_free(component_param_list_t* list);

#endif /* COMPONENT_PARAMS_H_ */
