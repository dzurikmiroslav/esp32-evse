#include "l_component.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <sys/queue.h>

#include "component_params.h"
#include "lauxlib.h"
#include "lua.h"
#include "script.h"

// static const char* TAG = "l_component";

typedef struct component_entry_s {
    char* id;
    char* name;
    char* description;
    int params_def_ref;
    int params_ref;
    int start_ref;
    int coroutine_ref;
    TickType_t resume_after;

    SLIST_ENTRY(component_entry_s) entries;
} component_entry_t;

typedef SLIST_HEAD(component_list_s, component_entry_s) component_list_t;

static int components_ref = LUA_NOREF;

const char* param_list_get_value(const component_param_list_t* list, const char* key)
{
    component_param_entry_t* entry;
    SLIST_FOREACH (entry, list, entries) {
        if (strcmp(key, entry->key) == 0) {
            return entry->value;
        }
    }
    return NULL;
}

static script_component_param_type_t cfg_entry_type_from_str(const char* type)
{
    if (type) {
        if (strcmp("string", type) == 0) {
            return SCRIPT_COMPONENT_PARAM_TYPE_STRING;
        }
        if (strcmp("number", type) == 0) {
            return SCRIPT_COMPONENT_PARAM_TYPE_NUMBER;
        }
        if (strcmp("boolean", type) == 0) {
            return SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN;
        }
    }
    return SCRIPT_COMPONENT_PARAM_TYPE_NONE;
}

/**
 * Process params defintion at top of stack, push params value and pop stack defintion
 */
static void l_process_params(lua_State* L, component_param_list_t* param_list)
{
    if (lua_istable(L, -1)) {
        lua_newtable(L);
        int params_idx = lua_gettop(L);

        lua_pushnil(L);
        while (lua_next(L, -3)) {
            int arg_def_idx = lua_gettop(L);
            int arg_key_idx = arg_def_idx - 1;

            if (lua_istable(L, arg_def_idx)) {
                lua_getfield(L, arg_def_idx, "type");
                script_component_param_type_t type = cfg_entry_type_from_str(lua_tostring(L, -1));
                lua_pop(L, 1);

                if (type != SCRIPT_COMPONENT_PARAM_TYPE_NONE) {
                    lua_pushvalue(L, -2);

                    const char* str_value = param_list ? param_list_get_value(param_list, lua_tostring(L, arg_key_idx)) : NULL;
                    if (str_value) {
                        switch (type) {
                        case SCRIPT_COMPONENT_PARAM_TYPE_STRING:
                            lua_pushstring(L, str_value);
                            break;
                        case SCRIPT_COMPONENT_PARAM_TYPE_NUMBER:
                            if (strlen(str_value) > 0) {
                                lua_pushnumber(L, atoi(str_value));
                            } else {
                                lua_pushnil(L);
                            }
                            break;
                        case SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN:
                            lua_pushboolean(L, strcmp("true", str_value) == 0);
                            break;
                        default:
                            lua_pushnil(L);
                            break;
                        }
                    } else {
                        lua_getfield(L, arg_def_idx, "default");
                    }

                    lua_settable(L, params_idx);
                }
            }
            lua_pop(L, 1);
        }
    } else {
        lua_pushnil(L);
    }

    lua_remove(L, -2);
}

static void component_process_params(lua_State* L, component_entry_t* component)
{
    luaL_unref(L, LUA_REGISTRYINDEX, component->params_ref);

    component_param_list_t* param_list = component_params_read(component->id);

    lua_rawgeti(L, LUA_REGISTRYINDEX, component->params_def_ref);

    l_process_params(L, param_list);
    component->params_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    if (param_list) component_params_free(param_list);
}

static void component_restart_coroutine(lua_State* L, component_entry_t* component)
{
    // end previous coroutine
    lua_rawgeti(L, LUA_REGISTRYINDEX, component->coroutine_ref);
    if (lua_isthread(L, -1)) {
        lua_State* co = lua_tothread(L, -1);
        if (lua_status(co) == LUA_YIELD) {
            lua_closethread(L, co);
        } else {
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    luaL_unref(L, LUA_REGISTRYINDEX, component->coroutine_ref);
    component->coroutine_ref = LUA_NOREF;
    component->resume_after = 0;

    lua_gc(L, LUA_GCCOLLECT, 0);

    // start new coroutine
    lua_rawgeti(L, LUA_REGISTRYINDEX, component->start_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, component->params_ref);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        lua_writestring(err, strlen(err));
        lua_writeline();
        lua_pop(L, 1);
    } else {
        if (lua_isthread(L, -1)) {
            component->coroutine_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            lua_pop(L, 1);
        }
    }
}

static component_list_t* get_component_list(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, components_ref);
    component_list_t* component_list = (component_list_t*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    return component_list;
}

static int l_add_component(lua_State* L)
{
    luaL_argcheck(L, lua_istable(L, 1), 1, "must be table");

    lua_getfield(L, 1, "id");
    luaL_argcheck(L, lua_isstring(L, -1), 1, "no id field");
    const char* id = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "name");
    luaL_argcheck(L, lua_isstring(L, -1), 1, "no name field");
    const char* name = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "description");
    const char* description = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "start");
    luaL_argcheck(L, lua_isfunction(L, -1), 1, "no start function");
    lua_pop(L, 1);

    component_list_t* component_list = get_component_list(L);
    component_entry_t* component;
    SLIST_FOREACH (component, component_list, entries) {
        if (strcmp(id, component->id) == 0) {
            luaL_argerror(L, 1, "component with duplicate id");
        }
    }

    component = (component_entry_t*)malloc(sizeof(component_entry_t));
    component->id = strdup(id);
    component->name = strdup(name);
    component->description = description ? strdup(description) : NULL;
    lua_getfield(L, 1, "params");
    component->params_def_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_getfield(L, 1, "start");
    component->start_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    component->params_ref = LUA_NOREF;
    component->coroutine_ref = LUA_NOREF;

    component_process_params(L, component);
    component_restart_coroutine(L, component);

    SLIST_INSERT_HEAD(component_list, component, entries);

    return 0;
}

static int l_component_gc(lua_State* L)
{
    component_list_t* component_list = get_component_list(L);

    while (!SLIST_EMPTY(component_list)) {
        component_entry_t* component = SLIST_FIRST(component_list);
        SLIST_REMOVE_HEAD(component_list, entries);

        free((void*)component->id);
        free((void*)component->name);
        if (component->description) free((void*)component->description);
        free((void*)component);
    }

    return 0;
}

void l_component_register(lua_State* L)
{
    lua_pushcfunction(L, l_add_component);
    lua_setglobal(L, "addcomponent");

    component_list_t* components = (component_list_t*)lua_newuserdata(L, sizeof(component_list_t));
    SLIST_INIT(components);

    luaL_newmetatable(L, "component");
    lua_pushcfunction(L, l_component_gc);
    lua_setfield(L, -2, "__gc");

    lua_setmetatable(L, -2);

    components_ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

void l_component_resume(lua_State* L)
{
    component_list_t* component_list = get_component_list(L);
    component_entry_t* component;
    SLIST_FOREACH (component, component_list, entries) {
        if (component->resume_after == 0 || component->resume_after < xTaskGetTickCount()) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->coroutine_ref);
            if (lua_isthread(L, -1)) {
                lua_State* co = lua_tothread(L, -1);
                int status = lua_status(co);
                if (status == LUA_YIELD || status == LUA_OK) {
                    int nresults = 0;
                    status = lua_resume(co, L, 0, &nresults);
                    if (status == LUA_YIELD || status == LUA_OK) {
                        if (nresults > 0 && lua_isinteger(co, -nresults)) {
                            component->resume_after = xTaskGetTickCount() + pdMS_TO_TICKS(lua_tointeger(co, -nresults));
                        }
                    } else {
                        const char* err = lua_tostring(co, -1);
                        lua_writestring(err, strlen(err));
                        lua_writeline();
                    }
                    lua_pop(co, nresults);
                }
            }
            lua_pop(L, 1);
        }
    }
}

script_component_list_t* l_component_get_components(lua_State* L)
{
    script_component_list_t* list = (script_component_list_t*)malloc(sizeof(script_component_list_t));
    SLIST_INIT(list);

    component_list_t* component_list = get_component_list(L);
    component_entry_t* component;
    SLIST_FOREACH (component, component_list, entries) {
        script_component_entry_t* entry = (script_component_entry_t*)malloc(sizeof(script_component_entry_t));
        entry->id = strdup(component->id);
        entry->name = strdup(component->name);
        entry->description = component->description ? strdup(component->description) : NULL;
        SLIST_INSERT_HEAD(list, entry, entries);
    }

    return list;
}

script_component_param_list_t* l_component_get_component_params(lua_State* L, const char* id)
{
    script_component_param_list_t* list = NULL;

    component_list_t* component_list = get_component_list(L);
    component_entry_t* component;
    SLIST_FOREACH (component, component_list, entries) {
        if (strcmp(id, component->id) == 0) {
            list = (script_component_param_list_t*)malloc(sizeof(script_component_param_list_t));
            SLIST_INIT(list);

            lua_rawgeti(L, LUA_REGISTRYINDEX, component->params_def_ref);
            int params_def_idx = lua_gettop(L);
            lua_rawgeti(L, LUA_REGISTRYINDEX, component->params_ref);
            int params_value_idx = lua_gettop(L);

            lua_pushnil(L);
            while (lua_next(L, params_def_idx)) {
                lua_getfield(L, -1, "type");
                script_component_param_type_t type = cfg_entry_type_from_str(lua_tostring(L, -1));
                lua_pop(L, 1);

                if (type != SCRIPT_COMPONENT_PARAM_TYPE_NONE) {
                    lua_getfield(L, -1, "name");
                    const char* name = lua_tostring(L, -1);
                    lua_pop(L, 1);

                    script_component_param_entry_t* entry = (script_component_param_entry_t*)malloc(sizeof(script_component_param_entry_t));
                    entry->key = strdup(lua_tostring(L, -2));
                    entry->type = type;
                    entry->name = strdup(name ? name : lua_tostring(L, -2));

                    lua_getfield(L, params_value_idx, entry->key);
                    switch (entry->type) {
                    case SCRIPT_COMPONENT_PARAM_TYPE_STRING:
                        const char* str_value = lua_tostring(L, -1);
                        entry->value.string = str_value ? strdup(str_value) : NULL;
                        break;
                    case SCRIPT_COMPONENT_PARAM_TYPE_NUMBER:
                        entry->value.number = lua_tonumber(L, -1);
                        break;
                    case SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN:
                        entry->value.boolean = lua_toboolean(L, -1);
                        break;
                    default:
                    }
                    lua_pop(L, 1);

                    SLIST_INSERT_HEAD(list, entry, entries);
                }
                lua_pop(L, 1);
            }

            lua_pop(L, 2);

            break;
        }
    }

    return list;
}

void l_component_restart(lua_State* L, const char* id)
{
    component_list_t* component_list = get_component_list(L);
    component_entry_t* component;
    SLIST_FOREACH (component, component_list, entries) {
        if (strcmp(id, component->id) == 0) {
            component_process_params(L, component);
            component_restart_coroutine(L, component);
            break;
        }
    }
}