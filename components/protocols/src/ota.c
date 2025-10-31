
#include "ota.h"

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <nvs.h>
#include <string.h>

#include "board_config.h"
#include "schedule_restart.h"

#define NVS_NAMESPACE "ota"
#define NVS_CHANNEL   "channel"

static const char* TAG = "ota";

static nvs_handle_t nvs;

void ota_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
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
    char channel_name[BOARD_CFG_OTA_CHANNEL_NAME_SIZE];
    ota_get_channel(channel_name);

    const char* channel_info_path = NULL;
    for (int i = 0; i < BOARD_CFG_OTA_CHANNEL_COUNT; i++) {
        if (board_cfg_is_ota_channel(board_config, i) && strcmp(channel_name, board_config.ota.channels[i].name) == 0) {
            channel_info_path = board_config.ota.channels[i].path;
            break;
        }
    }

    if (!channel_info_path) {
        ESP_LOGE(TAG, "Unknow channel: %s", channel_name);
        return ESP_ERR_NOT_FOUND;
    }

    // read channel info
    cJSON* json = http_get_json(channel_info_path);
    if (json == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    cJSON* json_item;
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

void ota_get_channel(char* value)
{
    size_t len = BOARD_CFG_OTA_CHANNEL_NAME_SIZE;
    if (nvs_get_str(nvs, NVS_CHANNEL, value, &len) != ESP_OK) {
        strcpy(value, board_config.ota.channels[0].name);
    }
}

void ota_set_channel(const char* value)
{
    nvs_set_str(nvs, NVS_CHANNEL, value);
    nvs_commit(nvs);
}
