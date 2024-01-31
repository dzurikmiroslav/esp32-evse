#ifndef L_EVSE_LIB_H_
#define L_EVSE_LIB_H_

#include "lua.h"

int luaopen_evse(lua_State* L);

void l_evse_process(lua_State* L);

uint8_t l_evse_get_driver_count(lua_State* L);

void l_evse_get_driver(lua_State* L, uint8_t index);

#endif /* L_EVSE_LIB_H_ */
