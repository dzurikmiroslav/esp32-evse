set(srcs
    "src/protocols.c"
    "src/http.c"
    "src/http_rest.c"
    "src/http_json.c"
    "src/http_web.c"
    "src/http_dav.c"
    "src/modbus_tcp.c"
    "src/scheduler.c")

set(embed_files
    "web.cpio")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    EMBED_FILES "${embed_files}"
                    PRIV_REQUIRES nvs_flash esp_http_server esp_wifi esp_timer esp_https_ota driver app_update json vfs spiffs mbedtls
                    REQUIRES config network modbus script serial logger)
