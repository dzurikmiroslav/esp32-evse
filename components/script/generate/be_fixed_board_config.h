#include "be_constobj.h"

static be_define_const_map_slots(m_libboard_config_map) {
    { be_const_key(SERIAL_NONE, 2), be_const_int(BOARD_CONFIG_SERIAL_NONE) },
    { be_const_key(ENERGY_METER_CUR, -1), be_const_int(BOARD_CONFIG_ENERGY_METER_CUR) },
    { be_const_key(SERIAL_RS485, 6), be_const_int(BOARD_CONFIG_SERIAL_RS485) },
    { be_const_key(read, 1), be_const_func(m_read) },
    { be_const_key(SERIAL_UART, -1), be_const_int(BOARD_CONFIG_SERIAL_UART) },
    { be_const_key(ENERGY_METER_CUR_VLT, 4), be_const_int(BOARD_CONFIG_ENERGY_METER_CUR_VLT) },
    { be_const_key(ENERGY_METER_NONE, -1), be_const_int(BOARD_CONFIG_ENERGY_METER_NONE) },
};

static be_define_const_map(
    m_libboard_config_map,
    7
);

static be_define_const_module(
    m_libboard_config,
    "board_config"
);

BE_EXPORT_VARIABLE be_define_const_native_module(board_config);
