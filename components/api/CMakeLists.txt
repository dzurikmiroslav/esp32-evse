set(srcs
    "src/api.c"
    "src/ota.c"
    "src/json.c"
    "src/timeout_utils.c"
    "src/modbus.c"
    "src/at_cmds.c"
    "lib/cAT/src/cat.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include" "lib/cAT/src"
                    PRIV_INCLUDE_DIRS "src"
                    PRIV_REQUIRES nvs_flash app_update json driver esp_http_client esp_netif esp_wifi esp_timer esp_hw_support
                    REQUIRES network config evse peripherals protocols serial script)

set_source_files_properties(lib/cAT/src/cat.c PROPERTIES COMPILE_FLAGS -Wno-maybe-uninitialized)