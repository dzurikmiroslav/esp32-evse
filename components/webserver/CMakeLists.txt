set(srcs
    "webserver.c")

set(embed_files
    "web-gz/index.html"
    "web-gz/settings.html"
    "web-gz/management.html"
    "web-gz/about.html"
    "web-gz/bootstrap.min.css"
    "web-gz/jquery.min.js"
    "web-gz/bootstrap.bundle.min.js"
    "web-gz/favicon.ico")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "${embed_files}"
                    PRIV_REQUIRES nvs_flash esp_http_server app_update json 
                    REQUIRES config evse)