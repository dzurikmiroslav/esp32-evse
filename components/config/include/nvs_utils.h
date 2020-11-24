#ifndef STORAGE_H_
#define STORAGE_H_

#include "nvs.h"

#define NVS_WIFI_ENABLED        "wifi_enabled"
#define NVS_WIFI_SSID           "wifi_ssid"
#define NVS_WIFI_PASSWORD       "wifi_passwd"

#define OTA_CHANNEL             "ota_channel"

extern nvs_handle nvs; // defined in main.c

#endif /* STORAGE_H_ */
