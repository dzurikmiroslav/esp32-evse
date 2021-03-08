#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp32-evse

include $(IDF_PATH)/make/project.mk

ifdef CONFIG_BOARD_CONFIG_DEPLOY
CFG_SRC_DIR = $(shell pwd)/cfg/${CONFIG_BOARD_CONFIG}
ifneq ($(wildcard $(CFG_SRC_DIR)/board.cfg),)
$(eval $(call spiffs_create_partition_image,config,$(CFG_SRC_DIR),FLASH_IN_PROJECT))
else
$(error $(CFG_SRC_DIR)/board.cfg doesn't exist)
endif
endif
