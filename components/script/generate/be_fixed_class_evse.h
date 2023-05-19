#include "be_constobj.h"

static be_define_const_map_slots(class_evse_map) {
    { be_const_key(charging_current, -1), be_const_func(m_charging_current) },
    { be_const_key(STATE_C, -1), be_const_int(EVSE_STATE_C) },
    { be_const_key(enabled, 15), be_const_func(m_enabled) },
    { be_const_key(state, -1), be_const_func(m_state) },
    { be_const_key(remove_driver, -1), be_const_func(m_remove_driver) },
    { be_const_key(add_driver, -1), be_const_func(m_add_driver) },
    { be_const_key(STATE_B, -1), be_const_int(EVSE_STATE_B) },
    { be_const_key(voltage, -1), be_const_func(m_voltage) },
    { be_const_key(_ctx, 6), be_const_var(0) },
    { be_const_key(init, -1), be_const_func(m_init) },
    { be_const_key(power, 2), be_const_func(m_power) },
    { be_const_key(set_enabled, -1), be_const_func(m_set_enabled) },
    { be_const_key(current, 1), be_const_func(m_current) },
    { be_const_key(_process, 8), be_const_func(m_process) },
    { be_const_key(STATE_A, -1), be_const_int(EVSE_STATE_A) },
    { be_const_key(charging_time, 16), be_const_func(m_charging_time) },
    { be_const_key(consumption, -1), be_const_func(m_consumption) },
    { be_const_key(STATE_F, -1), be_const_int(EVSE_STATE_F) },
    { be_const_key(STATE_E, 3), be_const_int(EVSE_STATE_E) },
    { be_const_key(STATE_D, -1), be_const_int(EVSE_STATE_D) },
    { be_const_key(delay, -1), be_const_func(m_delay) },
    { be_const_key(set_charging_current, -1), be_const_func(m_set_charging_current) },
};

static be_define_const_map(
    class_evse_map,
    22
);

BE_EXPORT_VARIABLE be_define_const_class(
    class_evse,
    1,
    NULL,
    Evse
);
