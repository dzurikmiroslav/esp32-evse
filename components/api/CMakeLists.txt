set(srcs
    "src/ota_utils.c"
    "src/json_utils.c"
    "src/timeout_utils.c"
    "src/webserver.c"
    "src/wifi.c"
    "src/mqtt.c")

set(embed_files
    "web-gz/index.html"
    "web-gz/settings.html"
    "web-gz/management.html"
    "web-gz/about.html"
    "web-gz/bootstrap.min.css"
    "web-gz/jquery.min.js"
    "web-gz/bootstrap.bundle.min.js"
    "web-gz/favicon.ico"
    "server_certs/ca_cert.pem")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_INCLUDE_DIRS "src"
                    EMBED_FILES "${embed_files}"
                    PRIV_REQUIRES nvs_flash esp_http_server esp_http_client app_update esp_https_ota json mdns mqtt
                    REQUIRES config evse)
            