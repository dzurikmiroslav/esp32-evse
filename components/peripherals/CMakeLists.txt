set(srcs
    "src/led.c"
    "src/pilot.c"
    "src/proximity.c"
    "src/ac_relay.c"
    "src/energy_meter.c"
    "src/socket_lock.c"
    "src/rcm.c"
    "src/aux.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_adc_cal
                    REQUIRES config evse api)