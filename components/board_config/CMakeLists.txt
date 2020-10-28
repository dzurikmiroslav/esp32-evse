set(srcs
    "board_config.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include")