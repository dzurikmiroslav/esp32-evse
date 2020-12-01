#ifndef OTA_UTILS_H_
#define OTA_UTILS_H_

#include <stdbool.h>
#include "esp_err.h"

#define OTA_VERSION_URL "https://raw.githubusercontent.com/dzurikmiroslav/esp32-evse/firmware/version.txt"

#define OTA_FIRMWARE_URL "https://raw.githubusercontent.com/dzurikmiroslav/esp32-evse/firmware/esp32-evse.bin"

esp_err_t ota_get_available_version(char *version);

bool ota_is_newer_version(const char *actual, const char *available);

#endif /* OTA_UTILS_H_ */
