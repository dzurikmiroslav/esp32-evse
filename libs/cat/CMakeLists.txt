idf_component_register(SRC_DIRS "src"
                    INCLUDE_DIRS "include")

target_compile_definitions(${COMPONENT_LIB} PUBLIC CAT_UNSOLICITED_CMD_BUFFER_SIZE=3) 

set_source_files_properties(src/cat.c PROPERTIES COMPILE_FLAGS -Wno-maybe-uninitialized)