#include "be_constobj.h"

static be_define_const_map_slots(m_libevse_map) {
    { be_const_key(init, -1), be_const_func(m_module_init) },
};

static be_define_const_map(
    m_libevse_map,
    1
);

static be_define_const_module(
    m_libevse,
    "evse"
);

BE_EXPORT_VARIABLE be_define_const_native_module(evse);
