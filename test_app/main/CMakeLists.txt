list(APPEND EXTRA_COMPONENT_DIRS "../mocks")

set(embed_files
    "script.lua"
    "params.yaml"
    "board.yaml")

idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "../mocks/peripherals/include" "../../components/script/src"  "../../components/config/src"
                    EMBED_FILES "${embed_files}"
                    REQUIRES cmock evse script config
                    PRIV_REQUIRES nvs_flash esp_netif esp_wifi littlefs vfs json lua
                    WHOLE_ARCHIVE)