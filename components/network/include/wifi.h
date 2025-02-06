#ifndef WIFI_H_
#define WIFI_H_

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdbool.h>

#define WIFI_SSID_SIZE           32  // from wifi_config_t.sta.ssid
#define WIFI_PASSWORD_SIZE       64  // from wifi_config_t.sta.password
#define WIFI_SCAN_SCAN_LIST_SIZE 10

#define WIFI_AP_CONNECTED_BIT     BIT0
#define WIFI_AP_DISCONNECTED_BIT  BIT1
#define WIFI_STA_CONNECTED_BIT    BIT2
#define WIFI_STA_DISCONNECTED_BIT BIT3
#define WIFI_AP_MODE_BIT          BIT4
#define WIFI_STA_MODE_BIT         BIT5
#define WIFI_STA_SCAN_BIT         BIT6

typedef struct {
    char ssid[WIFI_SSID_SIZE];
    int rssi;
    bool auth;
} wifi_scan_ap_t;

/**
 * @brief WiFi event group WIFI_AP_CONNECTED_BIT | WIFI_AP_DISCONNECTED_BIT | WIFI_STA_CONNECTED_BIT | WIFI_STA_DISCONNECTED_BIT | WIFI_AP_MODE_BIT | WIFI_STA_MODE_BIT |
 * WIFI_STA_SCAN_BIT
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
 * @param ssid NULL value will be skiped, max length 32
 * @param password NULL value will be skiped, max length 64
 * @return esp_err_t
 */
esp_err_t wifi_set_config(bool enabled, const char* ssid, const char* password);

/**
 * @brief Get WiFi STA enabled, stored in NVS
 *
 * @return true
 * @return false
 */
bool wifi_is_enabled(void);

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
 * @brief Test if WiFi STA ssi, stored in NVS
 *
 * @return true
 * @return false
 */
bool wifi_has_ssid(void);

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

/**
 * @brief Get WiFI ip address str
 *
 * @param ap true means AP, false means STA
 * @param str at least char[15]
 */
void wifi_get_ip(bool ap, char* str);

/**
 * @brief Get WiFI map address str
 *
 * @param ap true means AP, false means STA
 * @param str at least char[17]
 */
void wifi_get_mac(bool ap, char* str);

#endif /* WIFI_H_ */
