
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unity.h>
#include <unity_fixture.h>

#include "board_config.h"
#include "component_params.h"
#include "evse.h"
#include "l_aux_lib.h"
#include "l_board_config_lib.h"
#include "l_component.h"
#include "l_evse_lib.h"
#include "l_json_lib.h"
#include "l_mqtt_lib.h"
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#include "script_utils.h"
#include "script_watchdog.h"

#define PARAMS_YAML "/usr/lua/params.yaml"

static lua_State* L = NULL;

extern const char script_lua_start[] asm("_binary_script_lua_start");
extern const char script_lua_end[] asm("_binary_script_lua_end");

extern const char params_yaml_start[] asm("_binary_params_yaml_start");
extern const char params_yaml_end[] asm("_binary_params_yaml_end");

TEST_GROUP(script);

TEST_SETUP(script)
{
    L = luaL_newstate();

    luaL_openlibs(L);

    luaL_requiref(L, "component", luaopen_component, 1);
    lua_pop(L, 1);

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

    mkdir("/usr/lua", 0777);
}

TEST_TEAR_DOWN(script)
{
    lua_close(L);
    L = NULL;
}

bool check_lua(void)
{
    int top = lua_gettop(L);
    if (top != 0 && lua_isstring(L, top)) {
        printf("Lua error: %s\n", lua_tostring(L, top));
    }
    return top == 0;
}

void params_insert(component_param_list_t* list, const char* key, const char* value)
{
    component_param_entry_t* entry = (component_param_entry_t*)malloc(sizeof(component_param_entry_t));
    entry->key = strdup(key);
    entry->value = value ? strdup(value) : NULL;
    SLIST_INSERT_HEAD(list, entry, entries);
}

int params_get_length(const component_param_list_t* list)
{
    int count = 0;
    component_param_entry_t* entry;

    SLIST_FOREACH (entry, list, entries) {
        count++;
    }

    return count;
}

int components_get_length(const script_component_list_t* list)
{
    int count = 0;
    script_component_entry_t* entry;

    SLIST_FOREACH (entry, list, entries) {
        count++;
    }

    return count;
}

const char* get_global_var(const char* var)
{
    static char value[64] = { 0 };

    lua_getglobal(L, var);
    strcpy(value, lua_tostring(L, -1));
    lua_pop(L, 1);

    return value;
}

TEST(script, watchdog)
{
    script_watchdog_init(L);

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "counter = 0"));

    TEST_ASSERT_NOT_EQUAL(LUA_OK, luaL_dostring(L, "while true do\ncounter = counter + 1\nend"));
    TEST_ASSERT_EQUAL(1, lua_gettop(L));
    TEST_ASSERT_TRUE(lua_isstring(L, 1));
    TEST_ASSERT_EQUAL_STRING("watchdog end execution", lua_tostring(L, 1));
    lua_pop(L, 1);

    lua_getglobal(L, "counter");
    int counter = lua_tointeger(L, -1);
    lua_pop(L, 1);
    TEST_ASSERT_GREATER_THAN(0, counter);

    // watchdog not yet reset
    TEST_ASSERT_NOT_EQUAL(LUA_OK, luaL_dostring(L, "for i = 1,1000 do\ncounter = counter + 1\nend"));
    TEST_ASSERT_EQUAL(1, lua_gettop(L));
    TEST_ASSERT_TRUE(lua_isstring(L, 1));
    TEST_ASSERT_EQUAL_STRING("watchdog end execution", lua_tostring(L, 1));
    lua_pop(L, 1);

    lua_getglobal(L, "counter");
    int counter2 = lua_tointeger(L, -1);
    lua_pop(L, 1);
    TEST_ASSERT_GREATER_THAN(counter, counter2);

    script_watchdog_reset();

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "print(counter)"));

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "for i = 1,1000 do\ncounter = counter + 1\nend"));

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "print(counter)"));
    lua_getglobal(L, "counter");
    int counter3 = lua_tointeger(L, -1);
    lua_pop(L, 1);

    TEST_ASSERT_EQUAL(counter2 + 1000, counter3);
}

TEST(script, component)
{
    // default values
    remove(PARAMS_YAML);

    luaL_loadbuffer(L, script_lua_start, script_lua_end - script_lua_start, script_lua_start);
    lua_pcall(L, 0, LUA_MULTRET, 0);

    TEST_ASSERT_TRUE(check_lua());
    TEST_ASSERT_EQUAL_STRING("start", get_global_var("stage"));
    TEST_ASSERT_EQUAL_STRING("default1", get_global_var("paramstring1"));
    TEST_ASSERT_EQUAL_STRING("123", get_global_var("paramnumber1"));
    TEST_ASSERT_EQUAL_STRING("true", get_global_var("paramboolean1"));

    script_component_list_t* component_list = l_component_get_components(L);
    TEST_ASSERT_NOT_NULL(component_list);
    TEST_ASSERT_EQUAL(1, components_get_length(component_list));

    script_component_entry_t* component = SLIST_FIRST(component_list);
    TEST_ASSERT_NOT_NULL(component);
    TEST_ASSERT_EQUAL_STRING("component1", component->id);
    TEST_ASSERT_EQUAL_STRING("Test component 1", component->name);
    TEST_ASSERT_EQUAL_STRING("The test component 1", component->description);
    script_components_free(component_list);

    script_component_param_list_t* component_params = l_component_get_component_params(L, "component1");

    script_component_param_entry_t* component_param;
    int count = 0;
    SLIST_FOREACH (component_param, component_params, entries) {
        TEST_ASSERT_NOT_EMPTY(component_param->key);
        if (strcmp("string1", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("String1", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_STRING, component_param->type);
            TEST_ASSERT_EQUAL_STRING("default1", component_param->value.string);
        } else if (strcmp("number1", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("number1", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_NUMBER, component_param->type);
            TEST_ASSERT_EQUAL(123, component_param->value.number);
        } else if (strcmp("boolean1", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("boolean1", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN, component_param->type);
            TEST_ASSERT_EQUAL(true, component_param->value.boolean);
        } else if (strcmp("string2", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("string2", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_STRING, component_param->type);
            TEST_ASSERT_NULL(component_param->value.string);
        } else {
            count -= 100;
        }
    }
    TEST_ASSERT_EQUAL(4, count);

    script_component_params_free(component_params);

    // values from yaml
    FILE* params_file = fopen(PARAMS_YAML, "w");
    fwrite(params_yaml_start, sizeof(char), params_yaml_end - params_yaml_start, params_file);
    fclose(params_file);

    l_component_restart(L, "component1");
    TEST_ASSERT_EQUAL(0, lua_gettop(L));

    TEST_ASSERT_EQUAL_STRING("value1", get_global_var("paramstring1"));
    TEST_ASSERT_EQUAL_STRING("321.0", get_global_var("paramnumber1"));
    TEST_ASSERT_EQUAL_STRING("false", get_global_var("paramboolean1"));

    component_params = l_component_get_component_params(L, "component1");
    count = 0;
    SLIST_FOREACH (component_param, component_params, entries) {
        TEST_ASSERT_NOT_EMPTY(component_param->key);
        if (strcmp("string1", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("String1", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_STRING, component_param->type);
            TEST_ASSERT_EQUAL_STRING("value1", component_param->value.string);
        } else if (strcmp("number1", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("number1", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_NUMBER, component_param->type);
            TEST_ASSERT_EQUAL(321, component_param->value.number);
        } else if (strcmp("boolean1", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("boolean1", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN, component_param->type);
            TEST_ASSERT_EQUAL(false, component_param->value.boolean);
        } else if (strcmp("string2", component_param->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("string2", component_param->name);
            TEST_ASSERT_EQUAL(SCRIPT_COMPONENT_PARAM_TYPE_STRING, component_param->type);
            TEST_ASSERT_EMPTY(component_param->value.string);
        } else {
            count -= 100;
        }
    }
    TEST_ASSERT_EQUAL(4, count);

    script_component_params_free(component_params);

    // test yield/resume
    l_component_resume(L);
    TEST_ASSERT_EQUAL_STRING("loop1", get_global_var("stage"));

    l_component_resume(L);
    TEST_ASSERT_EQUAL_STRING("loop1", get_global_var("stage"));

    vTaskStepTick(pdMS_TO_TICKS(1100));
    l_component_resume(L);
    TEST_ASSERT_EQUAL_STRING("loop2", get_global_var("stage"));

    vTaskStepTick(pdMS_TO_TICKS(1100));
    l_component_resume(L);
    TEST_ASSERT_EQUAL_STRING("loop3", get_global_var("stage"));

    l_component_restart(L, "component1");
    TEST_ASSERT_EQUAL_STRING("start", get_global_var("stage"));

    l_component_resume(L);
    TEST_ASSERT_EQUAL_STRING("loop1", get_global_var("stage"));

    vTaskStepTick(pdMS_TO_TICKS(1100));
    l_component_resume(L);
    TEST_ASSERT_EQUAL_STRING("loop2", get_global_var("stage"));

    TEST_ASSERT_EQUAL(0, lua_gettop(L));  // after all Lua stack should be empty
}

TEST(script, component_params)
{
    component_param_list_t* list;
    // no params.yaml exist yet
    remove(PARAMS_YAML);
    TEST_ASSERT_NULL(fopen(PARAMS_YAML, "r"));

    list = component_params_read("component1");
    TEST_ASSERT_NULL(list);

    // create new params.yaml
    list = (component_param_list_t*)malloc(sizeof(component_param_list_t));
    SLIST_INIT(list);
    params_insert(list, "component1_key1", "value1");
    params_insert(list, "component1_key2", "value2");
    params_insert(list, "component1_key3", NULL);

    component_params_write("component1", list);
    component_params_free(list);

    list = component_params_read("component1");
    TEST_ASSERT_NOT_NULL(list);

    int count = 0;
    component_param_entry_t* entry;
    SLIST_FOREACH (entry, list, entries) {
        TEST_ASSERT_NOT_EMPTY(entry->key);
        if (strcmp("component1_key1", entry->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("value1", entry->value);
        } else if (strcmp("component1_key2", entry->key) == 0) {
            count++;
            TEST_ASSERT_EQUAL_STRING("value2", entry->value);
        } else if (strcmp("component1_key3", entry->key) == 0) {
            count++;
            TEST_ASSERT_EMPTY(entry->value);
        } else {
            count -= 100;
        }
    }
    TEST_ASSERT_EQUAL(3, count);

    component_params_free(list);

    // update existing params.yaml
    list = (component_param_list_t*)malloc(sizeof(component_param_list_t));
    SLIST_INIT(list);
    params_insert(list, "component2_key1", "value1");

    component_params_write("component2", list);
    component_params_free(list);

    list = component_params_read("component1");
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL(3, params_get_length(list));
    component_params_free(list);

    list = component_params_read("component2");
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL(1, params_get_length(list));
    component_params_free(list);
}

TEST(script, evse)
{
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "evse = require(\"evse\")"));

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "ret = evse.getstate()"));
    lua_getglobal(L, "ret");
    TEST_ASSERT_TRUE(lua_isnumber(L, -1));
    TEST_ASSERT_EQUAL(evse_get_state(), lua_tointeger(L, -1));
    lua_pop(L, 1);

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "evse.setchargingcurrent(8.6)"));
    TEST_ASSERT_EQUAL(86, evse_get_charging_current());

    TEST_ASSERT_EQUAL(0, lua_gettop(L));  // after all Lua stack should be empty
}

TEST(script, config)
{
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "boardconfig = require(\"boardconfig\")"));

    lua_getglobal(L, "boardconfig");
    TEST_ASSERT_TRUE(lua_istable(L, -1));

    lua_getfield(L, -1, "devicename");
    TEST_ASSERT_TRUE(lua_isstring(L, -1));
    TEST_ASSERT_EQUAL_STRING(board_config.device_name, lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);

    TEST_ASSERT_EQUAL(0, lua_gettop(L));  // after all Lua stack should be emty
}

TEST(script, json)
{
    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "json = require(\"json\")"));

    lua_getglobal(L, "json");
    TEST_ASSERT_TRUE(lua_istable(L, -1));
    lua_pop(L, 1);

    // encode

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "ret = json.encode({number1 = 5, string1 = \"str\", boolean1 = true}, false)"));

    lua_getglobal(L, "ret");
    TEST_ASSERT_TRUE(lua_isstring(L, -1));
    cJSON* json = cJSON_Parse(lua_tostring(L, -1));
    TEST_ASSERT_NOT_NULL(json);
    lua_pop(L, 1);

    cJSON* value_json = cJSON_GetObjectItem(json, "number1");
    TEST_ASSERT_NOT_NULL(value_json);
    TEST_ASSERT_TRUE(cJSON_IsNumber(value_json));
    TEST_ASSERT_EQUAL(5, value_json->valuedouble);

    value_json = cJSON_GetObjectItem(json, "string1");
    TEST_ASSERT_NOT_NULL(value_json);
    TEST_ASSERT_TRUE(cJSON_IsString(value_json));
    TEST_ASSERT_EQUAL_STRING("str", value_json->valuestring);

    value_json = cJSON_GetObjectItem(json, "boolean1");
    TEST_ASSERT_NOT_NULL(value_json);
    TEST_ASSERT_TRUE(cJSON_IsTrue(value_json));

    cJSON_free(json);

    // decode

    TEST_ASSERT_EQUAL(LUA_OK, luaL_dostring(L, "ret = json.decode('{\"number2\": 5, \"string2\": \"str\", \"boolean2\": true }')"));
    lua_getglobal(L, "ret");

    TEST_ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "number2");
    TEST_ASSERT_TRUE(lua_isnumber(L, -1));
    TEST_ASSERT_EQUAL(5, lua_tonumber(L, -1));
    lua_pop(L, 1);

    TEST_ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "string2");
    TEST_ASSERT_TRUE(lua_isstring(L, -1));
    TEST_ASSERT_EQUAL_STRING("str", lua_tostring(L, -1));
    lua_pop(L, 1);

    TEST_ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "boolean2");
    TEST_ASSERT_TRUE(lua_isboolean(L, -1));
    TEST_ASSERT_EQUAL(true, lua_toboolean(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);

    TEST_ASSERT_EQUAL(0, lua_gettop(L));  // after all Lua stack should be empty
}

TEST_GROUP_RUNNER(script)
{
    RUN_TEST_CASE(script, watchdog);
    RUN_TEST_CASE(script, component);
    RUN_TEST_CASE(script, component_params);
    RUN_TEST_CASE(script, evse);
    RUN_TEST_CASE(script, config)
    RUN_TEST_CASE(script, json);
}