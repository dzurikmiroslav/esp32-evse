#include <time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_ota_ops.h"

#include "mqtt.h"
#include "json_utils.h"
#include "mqtt.h"
#include "wifi.h"
#include "timeout_utils.h"
#include "evse.h"
#include "board_config.h"

cJSON* json_get_evse_config(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "chargingCurrent", evse_get_chaging_current() / 10.0);

    return root;
}

void json_set_evse_config(cJSON *root)
{
    uint16_t charging_current = cJSON_GetObjectItem(root, "chargingCurrent")->valuedouble * 10;

    evse_set_chaging_current(charging_current);
}

cJSON* json_get_wifi_config(void)
{
    cJSON *root = cJSON_CreateObject();

    char str[32];

    cJSON_AddBoolToObject(root, "enabled", wifi_get_enabled());
    wifi_get_ssid(str);
    cJSON_AddStringToObject(root, "ssid", str);

    return root;
}

void json_set_wifi_config(cJSON *root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "wifiEnabled"));
    char *ssid = cJSON_GetObjectItem(root, "wifiSSID")->valuestring;
    char *password = cJSON_GetObjectItem(root, "wifiPassword")->valuestring;

    timeout_set_wifi_config(enabled, ssid, password);
}

cJSON* json_get_mqtt_config(void)
{
    cJSON *root = cJSON_CreateObject();

    char str[64];

    cJSON_AddBoolToObject(root, "enabled", mqtt_get_enabled());
    mqtt_get_server(str);
    cJSON_AddStringToObject(root, "server", str);
    mqtt_get_base_topic(str);
    cJSON_AddStringToObject(root, "baseTopic", str);
    mqtt_get_user(str);
    cJSON_AddStringToObject(root, "user", str);
    mqtt_get_password(str);
    cJSON_AddStringToObject(root, "password", str);
    cJSON_AddNumberToObject(root, "periodicity", mqtt_get_periodicity());

    return root;
}

void json_set_mqtt_config(cJSON *root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "enabled"));
    char *server = cJSON_GetObjectItem(root, "server")->valuestring;
    char *base_topic = cJSON_GetObjectItem(root, "baseTopic")->valuestring;
    char *user = cJSON_GetObjectItem(root, "user")->valuestring;
    char *password = cJSON_GetObjectItem(root, "password")->valuestring;
    uint16_t periodicity = cJSON_GetObjectItem(root, "periodicity")->valuedouble;

    mqtt_set_config(enabled, server, base_topic, user, password, periodicity);
}

cJSON* json_get_state(void)
{
    cJSON *root = cJSON_CreateObject();

    char state[2] = "A";
    state[0] = 'A' + evse_get_state();
    cJSON_AddStringToObject(root, "state", state);
    cJSON_AddNumberToObject(root, "error", evse_get_error());
    cJSON_AddNumberToObject(root, "elapsed", evse_get_session_elapsed());
    cJSON_AddNumberToObject(root, "consumption", evse_get_session_consumption());
    cJSON_AddNumberToObject(root, "actualPower", evse_get_session_actual_power());

    return root;
}

cJSON* json_get_info(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "uptime", clock() / CLOCKS_PER_SEC);
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    cJSON_AddStringToObject(root, "appVersion", app_desc->version);
    cJSON_AddStringToObject(root, "appDate", app_desc->date);
    cJSON_AddStringToObject(root, "appTime", app_desc->time);
    cJSON_AddStringToObject(root, "idfVersion", app_desc->idf_ver);
    cJSON_AddStringToObject(root, "chip", CONFIG_IDF_TARGET);
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(root, "chipCores", chip_info.cores);
    cJSON_AddNumberToObject(root, "chipRevision", chip_info.revision);
    uint8_t mac[6];
    char str[32];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    sprintf(str, MACSTR, MAC2STR(mac));
    cJSON_AddStringToObject(root, "mac", str);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, str, sizeof(str));
    cJSON_AddStringToObject(root, "ip", str);

    return root;
}

cJSON* json_get_board_config(void)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "maxChargingCurrent", board_config.max_charging_current);
    cJSON_AddNumberToObject(root, "cableLock", board_config.cable_lock);
    cJSON_AddNumberToObject(root, "energyMeter", board_config.energy_meter);

    return root;
}

