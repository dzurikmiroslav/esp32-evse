#ifndef WIFI_H_
#define WIFI_H_

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#define WIFI_SSID_SIZE     32  // from wifi_config_t.sta.ssid
#define WIFI_PASSWORD_SIZE 64  // from wifi_config_t.sta.password

#define WIFI_AP_CONNECTED_BIT     BIT0
#define WIFI_AP_DISCONNECTED_BIT  BIT1
#define WIFI_STA_CONNECTED_BIT    BIT2
#define WIFI_STA_DISCONNECTED_BIT BIT3
#define WIFI_AP_MODE_BIT          BIT4
#define WIFI_STA_MODE_BIT         BIT5
#define WIFI_STA_SCAN_BIT         BIT6

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
 * @brief Scan AP entry
 *
 */
typedef struct wifi_scan_ap_entry_s {
    char* ssid;
    int8_t rssi;
    bool auth;

    SLIST_ENTRY(wifi_scan_ap_entry_s) entries;
} wifi_scan_ap_entry_t;

/**
 * @brief Scan AP list
 *
 * @return typedef
 */
typedef SLIST_HEAD(wifi_scan_ap_list_s, wifi_scan_ap_entry_s) wifi_scan_ap_list_t;

/**
 * @brief Scan AP, return AP list
 *
 * @return wifi_scan_ap_list_t*
 */
wifi_scan_ap_list_t* wifi_scan_aps(void);

/**
 * @brief Free AP list with all entries
 *
 * @param list
 */
void wifi_scan_aps_free(wifi_scan_ap_list_t* list);

/**
 * @brief Fee AP list entry
 *
 * @param entry
 */
void wifi_scan_aps_entry_free(wifi_scan_ap_entry_t* entry);

/**
 * @brief Get WiFi STA ssid, string length 32, stored in NVS
 *
 * @param value
 */
void wifi_get_ssid(char* value);

/**
 * @brief Get WiFi AP ssid, string length 32
 *
 * @param value
 */
void wifi_get_ap_ssid(char* value);

/**
 * @brief Get WiFi STA password, string length 32, stored in NVS
 *
 * @param value
 */
void wifi_get_password(char* value);

/**
 * @brief Get WiFi STA rssi
 *
 */
int8_t wifi_get_rssi(void);

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
