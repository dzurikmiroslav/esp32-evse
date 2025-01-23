set(embed_files
    "board_esp32.yaml"
    "board_esp32s2.yaml"
    "board_esp32s3.yaml")

idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES yaml
                    EMBED_FILES "${embed_files}")