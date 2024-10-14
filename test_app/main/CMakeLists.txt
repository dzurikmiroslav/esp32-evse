list(APPEND EXTRA_COMPONENT_DIRS "../mocks")

idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "../mocks/peripherals/include" "../../components/script/lib/lua" "../../components/script/src"
                    REQUIRES cmock evse script
                    PRIV_REQUIRES nvs_flash
                    WHOLE_ARCHIVE)