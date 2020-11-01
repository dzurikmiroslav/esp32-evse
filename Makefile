#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp32-evse

include $(IDF_PATH)/make/project.mk

ifdef CONFIG_CFG_DEPLOY_SF
CFG_SRC_DIR = $(shell pwd)/cfg
ifneq ($(wildcard $(CFG_SRC_DIR)/board.cfg),)
$(eval $(call spiffs_create_partition_image,config,$(CFG_SRC_DIR),FLASH_IN_PROJECT))
else
$(error $(CFG_SRC_DIR)/board.cfg doesn't exist. Please create board.cfg in $(CFG_SRC_DIR))
endif
endif
