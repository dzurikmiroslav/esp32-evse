#include "discovery.h"

#include <esp_log.h>
#include <mdns.h>
#include <nvs.h>
#include <string.h>

#define NVS_NAMESPACE     "discovery"
#define NVS_HOSTNAME      "hostname"
#define NVS_INSTANCE_NAME "instance_name"

#define DEFAULT_HOSTNAME      "evse"
#define DEFAULT_INSTANCE_NAME "EVSE"

_Static_assert(MDNS_NAME_MAX_LEN >= DISCOVERY_NAME_SIZE, "Wrong discovery name size");

static const char* TAG = "discovery";

static nvs_handle_t nvs;

void discovery_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    ESP_ERROR_CHECK(mdns_init());

    char name[DISCOVERY_NAME_SIZE];

    discovery_get_hostname(name);
    mdns_hostname_set(name);

    discovery_get_instance_name(name);
    mdns_instance_name_set(name);

    ESP_ERROR_CHECK(mdns_service_add(name, "_http", "_tcp", 80, NULL, 0));
}

esp_err_t discovery_set_hostname(const char* value)
{
    if (value != NULL && strlen(value) > DISCOVERY_NAME_SIZE - 1) {
        ESP_LOGE(TAG, "Hostname out of range");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = mdns_hostname_set(value);
    if (ret == ESP_OK) {
        nvs_set_str(nvs, NVS_HOSTNAME, value);
    }

    return ret;
}

void discovery_get_hostname(char* value)
{
    size_t len = DISCOVERY_NAME_SIZE - 1;
    if (nvs_get_str(nvs, NVS_HOSTNAME, value, &len) != ESP_OK) {
        strcpy(value, DEFAULT_HOSTNAME);
    }
}

esp_err_t discovery_set_instance_name(const char* value)
{
    if (value != NULL && strlen(value) > DISCOVERY_NAME_SIZE - 1) {
        ESP_LOGE(TAG, "Instance name out of range");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = mdns_instance_name_set(value);
    if (ret == ESP_OK) {
        nvs_set_str(nvs, NVS_INSTANCE_NAME, value);
    }

    return ret;
}

void discovery_get_instance_name(char* value)
{
    size_t len = DISCOVERY_NAME_SIZE - 1;
    if (nvs_get_str(nvs, NVS_INSTANCE_NAME, value, &len) != ESP_OK) {
        strcpy(value, DEFAULT_INSTANCE_NAME);
    }
}