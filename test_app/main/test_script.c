
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <unity.h>
#include <unity_fixture.h>

#include "board_config.h"
#include "l_aux_lib.h"
#include "l_board_config_lib.h"
#include "l_evse_lib.h"
#include "l_json_lib.h"
#include "l_mqtt_lib.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

static lua_State* L = NULL;

TEST_GROUP(script);

TEST_SETUP(script)
{
    L = luaL_newstate();

    luaL_openlibs(L);

    luaL_requiref(L, "evse", luaopen_evse, 1);
    lua_pop(L, 1);

    luaL_requiref(L, "mqtt", luaopen_mqtt, 0);
    lua_pop(L, 1);

    luaL_requiref(L, "json", luaopen_json, 0);
    lua_pop(L, 1);

    luaL_requiref(L, "aux", luaopen_aux, 0);
    lua_pop(L, 1);

    luaL_requiref(L, "boardconfig", luaopen_board_config, 0);
    lua_pop(L, 1);

    lua_gc(L, LUA_GCSETPAUSE, 110);
    lua_gc(L, LUA_GCSETSTEPMUL, 200);
}

TEST_TEAR_DOWN(script)
{
    lua_close(L);
    L = NULL;
}

TEST(script, require)
{
    // TODO serious tests
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "evse = require(\"evse\")"));
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "mqtt = require(\"mqtt\")"));
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "json = require(\"json\")"));
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "aux = require(\"aux\")"));
}

TEST(script, board_config)
{
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "boardconfig = require(\"boardconfig\")"));

    lua_getglobal(L, "boardconfig");
    TEST_ASSERT_TRUE(lua_istable(L, -1));

    lua_getfield(L, -1, "devicename");
    TEST_ASSERT_TRUE(lua_isstring(L, -1));
    TEST_ASSERT_EQUAL_STRING(board_config.device_name, lua_tostring(L, -1));
}

TEST_GROUP_RUNNER(script)
{
    RUN_TEST_CASE(script, require);
    RUN_TEST_CASE(script, board_config)
}