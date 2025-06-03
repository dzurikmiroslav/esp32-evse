idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_netif esp_timer app_update cat
                    REQUIRES driver config evse modbus logger network)