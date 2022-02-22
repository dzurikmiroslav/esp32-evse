#ifndef WIFI_H_
#define WIFI_H_

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_DISCONNECTED_BIT       BIT1
#define WIFI_AP_MODE_BIT            BIT2
#define WIFI_STA_MODE_BIT           BIT3

/**
 * @brief WiFi event group WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT
 * 
 */
extern EventGroupHandle_t wifi_event_group;

/**
 * @brief Initialize WiFi
 * 
 */
void wifi_init(void);

/**
 * @brief Set WiFi config
 * 
 * @param enabled 
 * @param ssid NULL value will be skiped
 * @param password NULL value will be skiped
 * @return esp_err_t 
 */
esp_err_t wifi_set_config(bool enabled, const char* ssid, const char* password);

/**
 * @brief Get WiFi STA enabled, stored in NVS
 * 
 * @return true 
 * @return false 
 */
bool wifi_get_enabled(void);


/**
 * @brief Get WiFi STA ssid, string length 32, stored in NVS
 * 
 * @param value 
 */
void wifi_get_ssid(char* value);

/**
 * @brief Get WiFi STA password, string length 32, stored in NVS
 * 
 * @param value 
 */
void wifi_get_password(char* value);

/**
 * @brief Start WiFi AP mode
 * 
 */
void wifi_ap_start(void);

/**
 * @brief Stop WiFi AP mode
 * 
 */
void wifi_ap_stop(void);

#endif /* WIFI_H_ */
