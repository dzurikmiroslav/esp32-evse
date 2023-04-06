#include "be_constobj.h"

static be_define_const_map_slots(m_libaux_map) {
    { be_const_key(read, 1), be_const_func(m_read) },
    { be_const_key(analog_read, -1), be_const_func(m_analog_read) },
    { be_const_key(write, -1), be_const_func(m_write) },
};

static be_define_const_map(
    m_libaux_map,
    3
);

static be_define_const_module(
    m_libaux,
    "aux"
);

BE_EXPORT_VARIABLE be_define_const_native_module(aux);
