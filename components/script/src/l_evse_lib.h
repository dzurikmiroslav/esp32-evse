#ifndef L_EVSE_LIB_H_
#define L_EVSE_LIB_H_

#include "lua.h"

int luaopen_evse(lua_State* L);

void l_evse_process(lua_State* L);

#endif /* L_EVSE_LIB_H_ */
