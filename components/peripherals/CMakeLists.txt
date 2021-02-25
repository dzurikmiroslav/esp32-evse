set(srcs
    "src/led.c"
    "src/pilot.c"
    "src/relay.c"
    "src/energy_meter.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_adc_cal
                    REQUIRES config)