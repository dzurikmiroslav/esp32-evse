idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES restart esp_timer serial evse app_update
                    REQUIRES cat)