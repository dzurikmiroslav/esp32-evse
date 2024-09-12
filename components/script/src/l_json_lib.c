#include "l_json_lib.h"

#include <cJSON.h>
#include <stdbool.h>

#include "lauxlib.h"
#include "lua.h"

static void decode_child(lua_State* L, cJSON* obj)
{
    if (cJSON_IsBool(obj)) {
        lua_pushboolean(L, cJSON_IsTrue(obj));
    } else if (cJSON_IsNumber(obj)) {
        lua_pushnumber(L, cJSON_GetNumberValue(obj));
    } else if (cJSON_IsString(obj)) {
        lua_pushstring(L, cJSON_GetStringValue(obj));
    } else if (cJSON_IsObject(obj)) {
        lua_newtable(L);
        cJSON* child = obj->child;
        while (child) {
            lua_pushstring(L, child->string);
            decode_child(L, child);
            lua_settable(L, -3);

            child = child->next;
        }
    } else if (cJSON_IsArray(obj)) {
        lua_newtable(L);
        cJSON* child = obj->child;
        int array_index = 1;
        while (child) {
            decode_child(L, child);
            lua_rawseti(L, -2, array_index++);

            child = child->next;
        }
    } else {
        lua_pushnil(L);
    }
}

static cJSON* encode_child(lua_State* L)
{
    cJSON* obj;

    if (lua_isboolean(L, -1)) {
        obj = cJSON_CreateBool(lua_toboolean(L, -1));
    } else if (lua_isnumber(L, -1)) {
        obj = cJSON_CreateNumber(lua_tonumber(L, -1));
    } else if (lua_isstring(L, -1)) {
        obj = cJSON_CreateString(lua_tostring(L, -1));
    } else if (lua_istable(L, -1)) {
        int len = lua_rawlen(L, -1);
        if (len > 0) {
            // table as array
            obj = cJSON_CreateArray();
            for (int i = 1; i <= len; i++) {
                lua_rawgeti(L, -1, i);
                cJSON_AddItemToArray(obj, encode_child(L));
                lua_pop(L, 1);
            }
        } else {
            // table as object
            obj = cJSON_CreateObject();
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                cJSON_AddItemToObject(obj, lua_tostring(L, -2), encode_child(L));
                lua_pop(L, 1);
            }
        }
    } else {
        obj = cJSON_CreateNull();
    }

    return obj;
}

static int l_decode(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);

    cJSON* root = cJSON_Parse(str);

    if (root == NULL) {
        luaL_error(L, "cant decode json");
    }

    decode_child(L, root);

    cJSON_Delete(root);

    return 1;
}

static int l_encode(lua_State* L)
{
    luaL_checkany(L, 1);

    bool format = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        format = lua_toboolean(L, 2);
    }

    lua_pushvalue(L, 1);
    cJSON* root = encode_child(L);
    lua_pop(L, 1);

    char* json;
    if (format) {
        json = cJSON_Print(root);
    } else {
        json = cJSON_PrintUnformatted(root);
    }

    cJSON_Delete(root);

    lua_pushstring(L, json);

    free((void*)json);

    return 1;
}

static const luaL_Reg lib[] = {
    { "decode", l_decode },
    { "encode", l_encode },
    { NULL, NULL },
};

int luaopen_json(lua_State* L)
{
    luaL_newlib(L, lib);

    return 1;
}