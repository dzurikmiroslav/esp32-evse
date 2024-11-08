set(srcs
    "src/serial.c"
    "src/serial_logger.c"
    "src/serial_modbus.c"
    "src/serial_nextion.c"
    "src/serial_script.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_netif esp_timer app_update
                    REQUIRES driver config evse modbus logger)