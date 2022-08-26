set(srcs
    "src/led.c"
    "src/pilot.c"
    "src/proximity.c"
    "src/ac_relay.c"
    "src/energy_meter.c"
    "src/socket_lock.c"
    "src/rcm.c"
    "src/aux.c"
    "src/serial.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_adc_cal nvs_flash
                    REQUIRES config evse api)