#include "l_board_config_lib.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "board_config.h"
#include "lauxlib.h"
#include "lua.h"

#define ENERGY_METER_NONE    0
#define ENERGY_METER_CUR     1
#define ENERGY_METER_CUR_VLT 2

int luaopen_board_config(lua_State* L)
{
    lua_createtable(L, 0, 20);

    lua_pushinteger(L, ENERGY_METER_NONE);
    lua_setfield(L, -2, "ENERGYMETERNONE");

    lua_pushinteger(L, ENERGY_METER_CUR);
    lua_setfield(L, -2, "ENERGYMETERCUR");

    lua_pushinteger(L, ENERGY_METER_CUR_VLT);
    lua_setfield(L, -2, "ENERGYMETERCURVLT");

    lua_pushinteger(L, BOARD_CFG_SERIAL_TYPE_NONE);
    lua_setfield(L, -2, "SERIALTYPENONE");

    lua_pushinteger(L, BOARD_CFG_SERIAL_TYPE_UART);
    lua_setfield(L, -2, "SERIALTYPEUART");

    lua_pushinteger(L, BOARD_CFG_SERIAL_TYPE_RS485);
    lua_setfield(L, -2, "SERIALTYPERS485");

    lua_pushstring(L, board_config.device_name);
    lua_setfield(L, -2, "devicename");

    lua_pushboolean(L, board_cfg_is_proximity(board_config));
    lua_setfield(L, -2, "proximity");

    lua_pushboolean(L, board_cfg_is_socket_lock(board_config));
    lua_setfield(L, -2, "socketlock");

    lua_pushboolean(L, board_cfg_is_rcm(board_config));
    lua_setfield(L, -2, "rcm");

    int energy_meter = ENERGY_METER_NONE;
    bool energy_meter_three_phases = false;
    if (board_cfg_is_energy_meter_cur(board_config)) {
        if (board_cfg_is_energy_meter_vlt(board_config)) {
            energy_meter = ENERGY_METER_CUR_VLT;
            energy_meter_three_phases = board_cfg_is_energy_meter_cur_3p(board_config) && board_cfg_is_energy_meter_vlt_3p(board_config);
        } else {
            energy_meter = ENERGY_METER_CUR;
            energy_meter_three_phases = board_cfg_is_energy_meter_vlt_3p(board_config);
        }
    }

    lua_pushinteger(L, energy_meter);
    lua_setfield(L, -2, "energymeter");

    lua_pushboolean(L, energy_meter_three_phases);
    lua_setfield(L, -2, "energymeterthreephases");

    lua_newtable(L);
    for (int i = 0; i < BOARD_CFG_SERIAL_COUNT; i++) {
        lua_pushinteger(L, board_config.serials[i].type);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "serials");

    lua_pushboolean(L, board_cfg_is_onewire(board_config) && board_config.onewire.temp_sensor);
    lua_setfield(L, -2, "temperaturesensor");

    lua_newtable(L);
    for (int i = 0; i < BOARD_CFG_AUX_INPUT_COUNT; i++) {
        if (board_cfg_is_aux_input(board_config, i)) {
            lua_pushstring(L, board_config.aux.inputs[i].name);
            lua_rawseti(L, -2, i + 1);
        }
    }
    lua_setfield(L, -2, "auxinputs");

    lua_newtable(L);
    for (int i = 0; i < BOARD_CFG_AUX_OUTPUT_COUNT; i++) {
        if (board_cfg_is_aux_output(board_config, i)) {
            lua_pushstring(L, board_config.aux.outputs[i].name);
            lua_rawseti(L, -2, i + 1);
        }
    }
    lua_setfield(L, -2, "auxoutputs");

    lua_newtable(L);
    for (int i = 0; i < BOARD_CFG_AUX_ANALOG_INPUT_COUNT; i++) {
        if (board_cfg_is_aux_analog_input(board_config, i)) {
            lua_pushstring(L, board_config.aux.analog_inputs[i].name);
            lua_rawseti(L, -2, i + 1);
        }
    }
    lua_setfield(L, -2, "auxanaloginputs");

    return 1;
}