set(srcs
    "peripherals.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_adc_cal
                    REQUIRES config)