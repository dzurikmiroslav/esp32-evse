set(COMPONENT_SRCS "main.c"
                   "peripherals.c"
                   "evse.c"
                   "webserver.c")
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

if(CONFIG_WEB_DEPLOY_SF)
    set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../web")
    if(EXISTS ${WEB_SRC_DIR}/dist/web-app-gz)
        spiffs_create_partition_image(www ${WEB_SRC_DIR}/dist/web-app-gz FLASH_IN_PROJECT)
    else()
        message(FATAL_ERROR "${WEB_SRC_DIR}/dist/web-app doesn't exit. Please run 'npm run build' in ${WEB_SRC_DIR}")
    endif()
endif()