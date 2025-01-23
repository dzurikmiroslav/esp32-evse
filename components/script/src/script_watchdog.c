#include "script_watchdog.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "lauxlib.h"

#define TIMEOUT 100

static TickType_t reset_tick;

static void watchdog_hook(lua_State *L, lua_Debug *ar)
{
    if (xTaskGetTickCount() > reset_tick + pdMS_TO_TICKS(TIMEOUT)) {
        luaL_error(L, "watchdog end execution");
    }
}

void script_watchdog_init(lua_State *L)
{
    lua_sethook(L, watchdog_hook, LUA_MASKCOUNT, 1000);
    script_watchdog_reset();
}

void script_watchdog_reset(void)
{
    reset_tick = xTaskGetTickCount();
}
