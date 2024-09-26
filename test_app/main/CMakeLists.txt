list(APPEND EXTRA_COMPONENT_DIRS "../mocks")

idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "../mocks/peripherals/include"
                    REQUIRES cmock evse
                    PRIV_REQUIRES nvs_flash)