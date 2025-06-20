
#include "ota.h"

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <string.h>
#include <time.h>

#define OTA_CHANELS_JSON_URL "https://dzurikmiroslav.github.io/esp32-evse/ota/" CONFIG_IDF_TARGET ".json"
#define OTA_DEFAULT_CHANEL   "stable"

#define NVS_NAMESPACE "ota"
#define NVS_CHANNEL   "channel"

static const char* TAG = "ota";

static nvs_handle_t nvs;

void ota_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
}

static void restart_func(void* arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
    vTaskDelete(NULL);
}

void schedule_restart(void)
{
    xTaskCreate(restart_func, "restart_task", 2 * 1024, NULL, 10, NULL);
}

static void http_client_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

cJSON* http_get_json(const char* url)
{
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }

    char* str = NULL;

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length > 0) {
        str = (char*)malloc(sizeof(char) * (content_length + 1));
        if (str == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory");
        } else {
            esp_http_client_read(client, str, content_length);
            str[content_length] = '\0';
        }
    }

    http_client_cleanup(client);

    cJSON* json = cJSON_Parse(str);
    free((char*)str);

    return json;
}

esp_err_t ota_get_available(char** version, char** path)
{
    // read channels
    cJSON* json = http_get_json(OTA_CHANELS_JSON_URL);
    if (json == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    char* version_url = NULL;
    char channel[OTA_CHANNEL_SIZE];
    ota_get_channel(channel);
    cJSON* json_item = cJSON_GetObjectItem(json, channel);
    if (cJSON_IsString(json_item)) {
        version_url = strdup(cJSON_GetStringValue(json_item));
    }

    cJSON_free(json);

    // read version & path
    json = http_get_json(version_url);
    if (json == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    if (version) {
        json_item = cJSON_GetObjectItem(json, "version");
        if (cJSON_IsString(json_item)) {
            *version = strdup(cJSON_GetStringValue(json_item));
        }
    }

    if (path) {
        json_item = cJSON_GetObjectItem(json, "path");
        if (cJSON_IsString(json_item)) {
            *path = strdup(cJSON_GetStringValue(json_item));
        }
    }

    cJSON_free(json);

    return ESP_OK;
}

ota_channel_list_t* ota_get_channel_list(void)
{
    ota_channel_list_t* list = (ota_channel_list_t*)malloc(sizeof(ota_channel_list_t));
    SLIST_INIT(list);

    ota_channel_entry_t* entry = (ota_channel_entry_t*)malloc(sizeof(ota_channel_entry_t));
    entry->channel = strdup("testing");
    SLIST_INSERT_HEAD(list, entry, entries);

    entry = (ota_channel_entry_t*)malloc(sizeof(ota_channel_entry_t));
    entry->channel = strdup("bleeding-edge");
    SLIST_INSERT_HEAD(list, entry, entries);

    entry = (ota_channel_entry_t*)malloc(sizeof(ota_channel_entry_t));
    entry->channel = strdup("stable");
    SLIST_INSERT_HEAD(list, entry, entries);

    return list;
}

void ota_channel_list_free(ota_channel_list_t* list)
{
    while (!SLIST_EMPTY(list)) {
        ota_channel_entry_t* item = SLIST_FIRST(list);

        SLIST_REMOVE_HEAD(list, entries);

        if (item->channel) free((void*)item->channel);
        free((void*)item);
    }

    free((void*)list);
}

void ota_get_channel(char* value)
{
    size_t len = OTA_CHANNEL_SIZE;
    if (nvs_get_str(nvs, NVS_CHANNEL, value, &len) != ESP_OK) {
        strcpy(value, OTA_DEFAULT_CHANEL);
    }
}

void ota_set_channel(const char* value)
{
    nvs_set_str(nvs, NVS_CHANNEL, value);
    nvs_commit(nvs);
}
