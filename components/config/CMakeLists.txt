set(srcs
    "board_config.c")

idf_component_register(SRCS "${srcs}"
					PRIV_REQUIRES nvs_flash
                    INCLUDE_DIRS "include")