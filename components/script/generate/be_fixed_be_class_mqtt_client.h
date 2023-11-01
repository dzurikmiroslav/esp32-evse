#include "be_constobj.h"

static be_define_const_map_slots(be_class_mqtt_client_map) {
    { be_const_key(connected, 2), be_const_func(m_connected) },
    { be_const_key(connect, -1), be_const_func(m_connect) },
    { be_const_key(deinit, 6), be_const_func(m_deinit) },
    { be_const_key(subscribe, 4), be_const_func(m_subscribe) },
    { be_const_key(init, 7), be_const_func(m_init) },
    { be_const_key(set_on_message, -1), be_const_func(m_set_on_message) },
    { be_const_key(publish, -1), be_const_func(m_publish) },
    { be_const_key(_X2Ep, -1), be_const_var(0) },
};

static be_define_const_map(
    be_class_mqtt_client_map,
    8
);

BE_EXPORT_VARIABLE be_define_const_class(
    be_class_mqtt_client,
    1,
    NULL,
    mqtt_client
);
