#ifndef OTA_H
#define OTA_H

#include <esp_err.h>
#include <stdbool.h>

void ota_init(void);

void ota_get_channel(char* value);

void ota_set_channel(const char* value);

esp_err_t ota_get_available(char** version, char** path);

#endif /* OTA_H */