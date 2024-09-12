#ifndef WIFI_H_
#define WIFI_H_

#include <esp_err.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdbool.h>

#define WIFI_SCAN_SCAN_LIST_SIZE 10

#define WIFI_AP_CONNECTED_BIT     BIT0
#define WIFI_AP_DISCONNECTED_BIT  BIT1
#define WIFI_STA_CONNECTED_BIT    BIT2
#define WIFI_STA_DISCONNECTED_BIT BIT3
#define WIFI_AP_MODE_BIT          BIT4
#define WIFI_STA_MODE_BIT         BIT5

typedef struct {
    char ssid[32];
    int rssi;
    bool auth;
} wifi_scan_ap_t;

/**
 * @brief WiFi event group WIFI_AP_CONNECTED_BIT | WIFI_AP_DISCONNECTED_BIT | WIFI_STA_CONNECTED_BIT | WIFI_STA_DISCONNECTED_BIT | WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT
 *
 */
extern EventGroupHandle_t wifi_event_group;

/**
 * @brief Initialize WiFi
 *
 */
void wifi_init(void);

/**
 * @brief Return WiFi STA network interface
 *
 * @return esp_netif_t*
 */
esp_netif_t* wifi_get_sta_netif(void);

/**
 * @brief Return WiFi AP network interface
 *
 * @return esp_netif_t*
 */
esp_netif_t* wifi_get_ap_netif(void);

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
 * @brief Scan for AP
 *
 * @param scan_aps array with length WIFI_SCAN_SCAN_LIST_SIZE
 * @return uint16_t number of available AP
 */
uint16_t wifi_scan(wifi_scan_ap_t* scan_aps);

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
