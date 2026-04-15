idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_driver_gpio esp_netif esp_timer app_update at
                    REQUIRES esp_driver_uart config evse modbus logger network)