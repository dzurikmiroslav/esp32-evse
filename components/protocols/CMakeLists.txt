set(embed_files
    "web.cpio")

idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    EMBED_FILES "${embed_files}"
                    PRIV_REQUIRES nvs_flash esp_http_server esp_wifi esp_timer esp_https_ota driver app_update json vfs littlefs mbedtls
                    REQUIRES config network modbus script serial logger)
