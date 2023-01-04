#ifndef OTA_H_
#define OTA_H_

#include <stdbool.h>
#include "esp_err.h"

#define OTA_VERSION_URL "https://dzurikmiroslav.github.io/esp32-evse/firmware/version.txt"

#define OTA_FIRMWARE_URL "https://dzurikmiroslav.github.io/esp32-evse/firmware/"

esp_err_t ota_get_available_version(char* version);

bool ota_is_newer_version(const char* actual, const char* available);

#endif /* OTA_H_ */
