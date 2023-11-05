#include "be_constobj.h"

static be_define_const_map_slots(be_class_evse_map) {
    { be_const_key(enabled, -1), be_const_func(m_enabled) },
    { be_const_key(ERR_DIODE_SHORT_BIT, -1), be_const_int(EVSE_ERR_DIODE_SHORT_BIT) },
    { be_const_key(STATE_E, -1), be_const_int(EVSE_STATE_E) },
    { be_const_key(current, -1), be_const_func(m_current) },
    { be_const_key(STATE_C2, 21), be_const_int(EVSE_STATE_C2) },
    { be_const_key(ERR_RCM_TRIGGERED_BIT, 16), be_const_int(EVSE_ERR_RCM_TRIGGERED_BIT) },
    { be_const_key(init, 4), be_const_func(m_init) },
    { be_const_key(set_available, -1), be_const_func(m_set_available) },
    { be_const_key(ERR_UNLOCK_FAULT_BIT, -1), be_const_int(EVSE_ERR_UNLOCK_FAULT_BIT) },
    { be_const_key(power, 33), be_const_func(m_power) },
    { be_const_key(voltage, -1), be_const_func(m_voltage) },
    { be_const_key(STATE_B1, 13), be_const_int(EVSE_STATE_B1) },
    { be_const_key(set_enabled, -1), be_const_func(m_set_enabled) },
    { be_const_key(high_temperature, 19), be_const_func(m_high_temperature) },
    { be_const_key(low_temperature, -1), be_const_func(m_low_temperature) },
    { be_const_key(ERR_RCM_SELFTEST_FAULT_BIT, -1), be_const_int(EVSE_ERR_RCM_SELFTEST_FAULT_BIT) },
    { be_const_key(ERR_TEMPERATURE_HIGH_BIT, -1), be_const_int(EVSE_ERR_TEMPERATURE_HIGH_BIT) },
    { be_const_key(ERR_TEMPERATURE_FAULT_BIT, -1), be_const_int(EVSE_ERR_TEMPERATURE_FAULT_BIT) },
    { be_const_key(charging_current, 35), be_const_func(m_charging_current) },
    { be_const_key(charging_time, -1), be_const_func(m_charging_time) },
    { be_const_key(add_driver, -1), be_const_func(m_add_driver) },
    { be_const_key(consumption, -1), be_const_func(m_consumption) },
    { be_const_key(error, -1), be_const_func(m_error) },
    { be_const_key(_process, 5), be_const_func(m_process) },
    { be_const_key(STATE_A, 15), be_const_int(EVSE_STATE_A) },
    { be_const_key(STATE_D2, -1), be_const_int(EVSE_STATE_D2) },
    { be_const_key(STATE_D1, -1), be_const_int(EVSE_STATE_D1) },
    { be_const_key(STATE_F, 18), be_const_int(EVSE_STATE_F) },
    { be_const_key(set_charging_current, -1), be_const_func(m_set_charging_current) },
    { be_const_key(ERR_LOCK_FAULT_BIT, 25), be_const_int(EVSE_ERR_LOCK_FAULT_BIT) },
    { be_const_key(ERR_PILOT_FAULT_BIT, 0), be_const_int(EVSE_ERR_PILOT_FAULT_BIT) },
    { be_const_key(STATE_C1, -1), be_const_int(EVSE_STATE_C1) },
    { be_const_key(available, -1), be_const_func(m_available) },
    { be_const_key(delay, -1), be_const_func(m_delay) },
    { be_const_key(remove_driver, -1), be_const_func(m_remove_driver) },
    { be_const_key(state, -1), be_const_func(m_state) },
    { be_const_key(STATE_B2, -1), be_const_int(EVSE_STATE_B2) },
    { be_const_key(_X2Ep, 17), be_const_var(0) },
    { be_const_key(session_time, -1), be_const_func(m_session_time) },
};

static be_define_const_map(
    be_class_evse_map,
    39
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_evse,
    1,
    NULL,
    evse
);
