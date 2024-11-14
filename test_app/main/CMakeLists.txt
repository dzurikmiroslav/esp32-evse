list(APPEND EXTRA_COMPONENT_DIRS "../mocks")

set(embed_files
    "component1.lua"
    "params.yaml")

idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "../mocks/peripherals/include" "../../components/script/src"
                    EMBED_FILES "${embed_files}"
                    REQUIRES cmock evse script
                    PRIV_REQUIRES nvs_flash esp_netif esp_wifi spiffs vfs json lua
                    WHOLE_ARCHIVE)