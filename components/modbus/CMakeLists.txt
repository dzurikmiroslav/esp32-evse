idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    PRIV_REQUIRES nvs_flash app_update driver esp_timer
                    REQUIRES config restart evse peripherals)
