set(srcs
    "src/protocols.c"
    "src/rest.c"
    "src/mqtt.c"
    "src/modbus_tcp.c"
    "src/web_archive.c"
    "src/date_time.c")

set(embed_files
    "web.cpio"
    "server_certs/ca_cert.pem")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    EMBED_FILES "${embed_files}"
                    PRIV_REQUIRES nvs_flash esp_http_server esp_netif esp_https_ota app_update json mqtt vfs spiffs
                    REQUIRES config api script logger)
