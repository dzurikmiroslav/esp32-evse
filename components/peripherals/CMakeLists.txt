idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_driver_gpio esp_driver_ledc esp_adc esp_timer
                    REQUIRES config evse)