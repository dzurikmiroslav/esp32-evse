#ifndef RESTART_UTILS_H
#define RESTART_UTILS_H

#include <stdbool.h>

void timeout_restart();

void timeout_set_wifi_config(bool enabled, const char* ssid, const char* password);

#endif /* RESTART_UTILS_H */
