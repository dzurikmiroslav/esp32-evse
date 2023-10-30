#include "be_constobj.h"

static be_define_const_map_slots(be_class_mqtt_map) {
    { be_const_key(init, -1), be_const_func(m_init) },
    { be_const_key(_ctx, -1), be_const_var(0) },
    { be_const_key(deinit, 3), be_const_func(m_deinit) },
    { be_const_key(connect, -1), be_const_func(m_connect) },
    { be_const_key(publish, -1), be_const_func(m_publish) },
};

static be_define_const_map(
    be_class_mqtt_map,
    5
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_mqtt,
    1,
    NULL,
    mqtt
);
