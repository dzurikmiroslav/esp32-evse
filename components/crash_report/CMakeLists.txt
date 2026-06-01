idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_system esp_hw_support freertos)

# Intercept esp_panic_handler so we can capture the crash backtrace into RTC
# no-init memory before the normal panic/reboot flow runs. The flag must reach
# the final executable link, hence INTERFACE on the component library.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
