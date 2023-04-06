#include "be_constobj.h"

static be_define_const_map_slots(class_timer_map) {
    { be_const_key(delay, 5), be_const_func(m_delay) },
    { be_const_key(_X2Ep, -1), be_const_var(0) },
    { be_const_key(clear_interval, -1), be_const_func(m_clear_interval) },
    { be_const_key(init, 4), be_const_func(m_init) },
    { be_const_key(set_interval, -1), be_const_func(m_set_interval) },
    { be_const_key(process, -1), be_const_func(m_process) },
};

static be_define_const_map(
    class_timer_map,
    6
);

BE_EXPORT_VARIABLE be_define_const_class(
    class_timer,
    1,
    NULL,
    timer_class
);
