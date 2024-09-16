set(srcs
    "src/evse.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash
                    REQUIRES peripherals config)