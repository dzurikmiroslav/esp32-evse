idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include" 
                    PRIV_REQUIRES nvs_flash app_update json mqtt esp_timer lua yaml
                    REQUIRES config evse peripherals protocols serial logger)
