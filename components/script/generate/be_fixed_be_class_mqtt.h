#include "be_constobj.h"

static be_define_const_map_slots(be_class_mqtt_map) {
    { be_const_key(deinit, 1), be_const_func(m_deinit) },
    { be_const_key(init, -1), be_const_func(m_init) },
    { be_const_key(publish, 4), be_const_func(m_publish) },
    { be_const_key(connect, -1), be_const_func(m_connect) },
    { be_const_key(subscribe, -1), be_const_func(m_subscribe) },
    { be_const_key(_ctx, 0), be_const_var(0) },
    { be_const_key(connected, 2), be_const_func(m_connected) },
};

static be_define_const_map(
    be_class_mqtt_map,
    7
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_mqtt,
    1,
    NULL,
    mqtt
);
