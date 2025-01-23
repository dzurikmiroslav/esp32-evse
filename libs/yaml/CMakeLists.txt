idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include")

set_source_files_properties(src/emitter.c PROPERTIES COMPILE_FLAGS "-Wno-unused-value -Wno-return-type")
set_source_files_properties(src/api.c PROPERTIES COMPILE_FLAGS "-DHAVE_CONFIG_H")