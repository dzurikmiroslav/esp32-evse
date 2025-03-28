#ifndef OTA_H
#define OTA_H

#include <esp_err.h>
#include <stdbool.h>

#define OTA_VERSION_URL  "https://dzurikmiroslav.github.io/esp32-evse/firmware/version.txt"
#define OTA_FIRMWARE_URL "https://dzurikmiroslav.github.io/esp32-evse/firmware/"

esp_err_t ota_get_available_version(char* version);

bool ota_is_newer_version(const char* actual, const char* available);

void schedule_restart(void);

#endif /* OTA_H */