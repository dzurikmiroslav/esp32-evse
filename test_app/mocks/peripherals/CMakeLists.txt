idf_component_get_property(original_peripherals_dir peripherals COMPONENT_OVERRIDEN_DIR)

idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "${original_peripherals_dir}/include" "include")