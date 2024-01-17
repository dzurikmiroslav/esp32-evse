set(srcs
    "src/modbus.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    PRIV_REQUIRES nvs_flash app_update driver esp_timer
                    REQUIRES config evse peripherals)
