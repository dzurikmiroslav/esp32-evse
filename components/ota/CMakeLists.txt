set(srcs
    "ota.c")
    
set(embed_files
    "server_certs/ca_cert.pem")
    
idf_component_register(SRCS "${srcs}"
					EMBED_FILES "${embed_files}"
					PRIV_REQUIRES nvs_flash app_update esp_http_client
                    INCLUDE_DIRS "include")