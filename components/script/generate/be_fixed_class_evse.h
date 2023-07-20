#include "be_constobj.h"

static be_define_const_map_slots(class_evse_map) {
    { be_const_key(_process, -1), be_const_func(m_process) },
    { be_const_key(STATE_A, -1), be_const_int(EVSE_STATE_A) },
    { be_const_key(enabled, 22), be_const_func(m_enabled) },
    { be_const_key(available, -1), be_const_func(m_available) },
    { be_const_key(STATE_B2, -1), be_const_int(EVSE_STATE_B2) },
    { be_const_key(current, 11), be_const_func(m_current) },
    { be_const_key(state, 5), be_const_func(m_state) },
    { be_const_key(init, -1), be_const_func(m_init) },
    { be_const_key(remove_driver, -1), be_const_func(m_remove_driver) },
    { be_const_key(charging_current, -1), be_const_func(m_charging_current) },
    { be_const_key(_ctx, 7), be_const_var(0) },
    { be_const_key(charging_time, 26), be_const_func(m_charging_time) },
    { be_const_key(set_enabled, -1), be_const_func(m_set_enabled) },
    { be_const_key(voltage, -1), be_const_func(m_voltage) },
    { be_const_key(STATE_C2, 1), be_const_int(EVSE_STATE_C2) },
    { be_const_key(STATE_F, 0), be_const_int(EVSE_STATE_F) },
    { be_const_key(STATE_D1, -1), be_const_int(EVSE_STATE_D1) },
    { be_const_key(STATE_D2, -1), be_const_int(EVSE_STATE_D2) },
    { be_const_key(power, 2), be_const_func(m_power) },
    { be_const_key(set_available, 10), be_const_func(m_set_available) },
    { be_const_key(consumption, 3), be_const_func(m_consumption) },
    { be_const_key(session_time, -1), be_const_func(m_session_time) },
    { be_const_key(STATE_B1, -1), be_const_int(EVSE_STATE_B1) },
    { be_const_key(STATE_C1, -1), be_const_int(EVSE_STATE_C1) },
    { be_const_key(delay, 9), be_const_func(m_delay) },
    { be_const_key(set_charging_current, 12), be_const_func(m_set_charging_current) },
    { be_const_key(STATE_E, -1), be_const_int(EVSE_STATE_E) },
    { be_const_key(add_driver, 4), be_const_func(m_add_driver) },
};

static be_define_const_map(
    class_evse_map,
    28
);

BE_EXPORT_VARIABLE be_define_const_class(
    class_evse,
    1,
    NULL,
    Evse
);
