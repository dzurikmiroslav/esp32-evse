idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include")

set_source_files_properties(src/cat.c PROPERTIES COMPILE_FLAGS -Wno-maybe-uninitialized)