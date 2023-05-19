set(srcs
    "src/logger.c"
    "src/output_buffer.c"
    )

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "include")
