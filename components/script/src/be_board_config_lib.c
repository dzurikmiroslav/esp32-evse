#include "be_object.h"

#include "be_mapping_utils.h"
#include "board_config.h"


static int m_read(bvm* vm)
{
    be_newobject(vm, "map");
    be_map_insert_str(vm, "device_name", board_config.device_name);
    be_map_insert_bool(vm, "proximity", board_config.proximity);
    be_map_insert_bool(vm, "socket_lock", board_config.socket_lock);
    be_map_insert_bool(vm, "rcm", board_config.rcm);
    be_map_insert_int(vm, "energy_meter", board_config.energy_meter);
    be_map_insert_bool(vm, "energy_meter_three_phases", board_config.energy_meter_three_phases);
    be_map_insert_int(vm, "serial_1", board_config.serial_1);
    be_map_insert_int(vm, "serial_2", board_config.serial_2);
#if SOC_UART_NUM > 2
    be_map_insert_int(vm, "serial_3", board_config.serial_3);
#else
    be_map_insert_int(vm, "serial_3", BOARD_CONFIG_SERIAL_NONE);
#endif
    be_map_insert_bool(vm, "onewire", board_config.onewire);
    be_map_insert_bool(vm, "onewire_temp_sensor", board_config.onewire_temp_sensor);
    be_pop(vm, 1);
    be_return(vm);
}

/* @const_object_info_begin
module board_config (scope: global) {
    ENERGY_METER_NONE, int(BOARD_CONFIG_ENERGY_METER_NONE)
    ENERGY_METER_CUR, int(BOARD_CONFIG_ENERGY_METER_CUR)
    ENERGY_METER_CUR_VLT, int(BOARD_CONFIG_ENERGY_METER_CUR_VLT)
    SERIAL_NONE, int(BOARD_CONFIG_SERIAL_NONE)
    SERIAL_UART, int(BOARD_CONFIG_SERIAL_UART)
    SERIAL_RS485, int(BOARD_CONFIG_SERIAL_RS485)
    read, func(m_read)
}
@const_object_info_end */
#include "../generate/be_fixed_board_config.h"