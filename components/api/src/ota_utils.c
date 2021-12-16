#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "cJSON.h"

#include "ota_utils.h"

static const char* TAG = "ota";

extern const char server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const char server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static void http_client_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

esp_err_t ota_get_available_version(char* version)
{
    esp_http_client_config_t config = {
        .url = OTA_VERSION_URL,
        .cert_pem = server_cert_pem_start
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length > 0) {
        esp_http_client_read(client, version, content_length);
        version[content_length] = '\0';
        http_client_cleanup(client);
        return ESP_OK;
    } else {
        http_client_cleanup(client);
        ESP_LOGI(TAG, "No firmware available");
        return ESP_ERR_NOT_FOUND;
    }
}

bool ota_is_newer_version(const char* actual, const char* available)
{
    // available version has no suffix eg: vX.X.X-beta

    char actual_trimed[32];
    strcpy(actual_trimed, actual);

    char* saveptr;
    strtok_r(actual_trimed, "-", &saveptr);
    bool actual_has_suffix = strtok_r(NULL, "-", &saveptr);

    int diff = strcmp(available, actual_trimed);
    if (diff == 0) {
        return actual_has_suffix;
    } else {
        return diff > 0;
    }
}
