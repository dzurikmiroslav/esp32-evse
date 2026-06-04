idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES app_update
                    REQUIRES network config esp_system)

set_target_properties("__idf_esp_system" PROPERTIES COMPILE_FLAGS "-include ../components/logger/esp_system/weakprint.h")

target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")    
