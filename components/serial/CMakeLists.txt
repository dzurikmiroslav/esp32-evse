set(srcs
    "src/serial.c"
    "src/serial_logger.c"
    "src/serial_at.c"
    "src/serial_modbus.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash
                    REQUIRES config evse api)