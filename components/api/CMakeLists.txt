set(srcs
    "src/ota.c"
    "src/json.c"
    "src/timeout_utils.c"
    "src/modbus.c"
    "src/at_cmds.c"
    "lib/cAT/src/cat.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include" "lib/cAT/src"
                    PRIV_INCLUDE_DIRS "src"
                    PRIV_REQUIRES nvs_flash app_update json esp_http_client
                    REQUIRES network config evse peripherals protocols serial)

set_source_files_properties(lib/cAT/src/cat.c PROPERTIES COMPILE_FLAGS -Wno-maybe-uninitialized)