#ifndef WIFI_H_
#define WIFI_H_

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_DISCONNECTED_BIT       BIT1
#define WIFI_AP_MODE_BIT            BIT0
#define WIFI_STA_MODE_BIT           BIT1

extern EventGroupHandle_t wifi_event_group;         // WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT

extern EventGroupHandle_t wifi_ap_event_group;      // WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT

extern EventGroupHandle_t wifi_mode_event_group;    // WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT

void wifi_init(void);

void wifi_set_config(bool enabled, const char *ssid, const char *password);

bool wifi_get_enabled(void);

void wifi_get_ssid(char *value); //string length 32

void wifi_get_password(char *value); //string length 64

bool wifi_is_valid_config(void);

void wifi_ap_start(void);

void wifi_ap_stop(void);

#endif /* WIFI_H_ */
