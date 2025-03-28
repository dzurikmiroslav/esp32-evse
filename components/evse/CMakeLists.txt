idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash
                    REQUIRES peripherals config)