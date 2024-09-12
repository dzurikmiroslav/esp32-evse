#include "l_board_config_lib.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "board_config.h"
#include "lauxlib.h"
#include "lua.h"

int luaopen_board_config(lua_State* L)
{
    lua_createtable(L, 0, 20);

    lua_pushinteger(L, BOARD_CONFIG_ENERGY_METER_NONE);
    lua_setfield(L, -2, "ENERGYMETERNONE");

    lua_pushinteger(L, BOARD_CONFIG_ENERGY_METER_CUR);
    lua_setfield(L, -2, "ENERGYMETERCUR");

    lua_pushinteger(L, BOARD_CONFIG_ENERGY_METER_CUR_VLT);
    lua_setfield(L, -2, "ENERGYMETERCURVLT");

    lua_pushinteger(L, BOARD_CONFIG_SERIAL_NONE);
    lua_setfield(L, -2, "SERIALNONE");

    lua_pushinteger(L, BOARD_CONFIG_SERIAL_UART);
    lua_setfield(L, -2, "SERIALUART");

    lua_pushinteger(L, BOARD_CONFIG_SERIAL_RS485);
    lua_setfield(L, -2, "SERIALRS485");

    lua_pushstring(L, board_config.device_name);
    lua_setfield(L, -2, "devicename");

    lua_pushboolean(L, board_config.proximity);
    lua_setfield(L, -2, "proximity");

    lua_pushboolean(L, board_config.socket_lock);
    lua_setfield(L, -2, "socketlock");

    lua_pushboolean(L, board_config.rcm);
    lua_setfield(L, -2, "rcm");

    lua_pushinteger(L, board_config.energy_meter);
    lua_setfield(L, -2, "energymeter");

    lua_pushboolean(L, board_config.energy_meter_three_phases);
    lua_setfield(L, -2, "energymeterthreephases");

    lua_pushinteger(L, board_config.serial_1);
    lua_setfield(L, -2, "serial1");

    lua_pushinteger(L, board_config.serial_2);
    lua_setfield(L, -2, "serial2");

#if SOC_UART_NUM > 2
    lua_pushinteger(L, board_config.serial_3);
#else
    lua_pushinteger(L, BOARD_CONFIG_SERIAL_NONE);
#endif
    lua_setfield(L, -2, "serial3");

    lua_pushboolean(L, board_config.onewire);
    lua_setfield(L, -2, "onewire");

    lua_pushboolean(L, board_config.onewire_temp_sensor);
    lua_setfield(L, -2, "onewiretempsensor");

    lua_newtable(L);
    int idx = 1;
    if (board_config.aux_in_1) {
        lua_pushstring(L, board_config.aux_in_1_name);
        lua_rawseti(L, -2, idx++);
    }
    if (board_config.aux_in_2) {
        lua_pushstring(L, board_config.aux_in_2_name);
        lua_rawseti(L, -2, idx++);
    }
    if (board_config.aux_in_3) {
        lua_pushstring(L, board_config.aux_in_3_name);
        lua_rawseti(L, -2, idx++);
    }
    if (board_config.aux_in_4) {
        lua_pushstring(L, board_config.aux_in_4_name);
        lua_rawseti(L, -2, idx++);
    }
    lua_setfield(L, -2, "auxin");

    lua_newtable(L);
    idx = 1;
    if (board_config.aux_out_1) {
        lua_pushstring(L, board_config.aux_out_1_name);
        lua_rawseti(L, -2, idx++);
    }
    if (board_config.aux_out_2) {
        lua_pushstring(L, board_config.aux_out_2_name);
        lua_rawseti(L, -2, idx++);
    }
    if (board_config.aux_out_3) {
        lua_pushstring(L, board_config.aux_out_3_name);
        lua_rawseti(L, -2, idx++);
    }
    if (board_config.aux_out_4) {
        lua_pushstring(L, board_config.aux_out_4_name);
        lua_rawseti(L, -2, idx++);
    }
    lua_setfield(L, -2, "auxout");

    lua_newtable(L);
    idx = 1;
    if (board_config.aux_ain_1) {
        lua_pushstring(L, board_config.aux_ain_1_name);
        lua_rawseti(L, -2, idx++);
    }
    if (board_config.aux_ain_2) {
        lua_pushstring(L, board_config.aux_ain_2_name);
        lua_rawseti(L, -2, idx++);
    }
    lua_setfield(L, -2, "auxain");

    return 1;
}