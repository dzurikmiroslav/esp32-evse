#ifndef L_OS_EXT_LIB_H_
#define L_OS_EXT_LIB_H_

#include <stdbool.h>

#include "lua.h"
#include "script.h"

void l_component_register(lua_State* L);

void l_component_resume(lua_State* L, bool not_terminate);

script_component_list_t* l_component_get_components(lua_State* L);

script_component_param_list_t* l_component_get_component_params(lua_State* L, const char* id);

void l_component_restart(lua_State* L, const char* id);

#endif /* L_OS_EXT_LIB_H_ */
