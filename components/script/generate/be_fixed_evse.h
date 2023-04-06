#include "be_constobj.h"

static be_define_const_map_slots(m_libevse_map) {
    { be_const_key(set_enabled, 7), be_const_func(m_set_enabled) },
    { be_const_key(STATE_B, -1), be_const_int(EVSE_STATE_B) },
    { be_const_key(STATE_C, -1), be_const_int(EVSE_STATE_C) },
    { be_const_key(enabled, -1), be_const_func(m_enabled) },
    { be_const_key(STATE_D, -1), be_const_int(EVSE_STATE_D) },
    { be_const_key(STATE_E, -1), be_const_int(EVSE_STATE_E) },
    { be_const_key(STATE_F, -1), be_const_int(EVSE_STATE_F) },
    { be_const_key(STATE_A, 8), be_const_int(EVSE_STATE_A) },
    { be_const_key(state, -1), be_const_func(m_state) },
};

static be_define_const_map(
    m_libevse_map,
    9
);

static be_define_const_module(
    m_libevse,
    "evse"
);

BE_EXPORT_VARIABLE be_define_const_native_module(evse);
