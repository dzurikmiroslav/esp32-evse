set(COMPONENT_SRCS "main.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()

if(CONFIG_CFG_DEPLOY_SF)
    set(CFG_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cfg")
    if(EXISTS ${CFG_SRC_DIR}/board.cfg)
        spiffs_create_partition_image(cfg ${CFG_SRC_DIR} FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "${CFG_SRC_DIR}/board.cfg doesn't exit. Please create board.cfg in ${CFG_SRC_DIR}")
    endif()
endif()
