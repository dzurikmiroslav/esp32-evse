set(srcs
    "webserver.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    EMBED_FILES web/index.html
                    PRIV_REQUIRES nvs_flash esp_http_server json
                    REQUIRES config evse)