set(srcs
    "src/wifi.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_netif esp_wifi)