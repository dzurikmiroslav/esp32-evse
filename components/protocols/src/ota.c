
#include "ota.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <time.h>

static const char* TAG = "ota";

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

esp_err_t ota_get_available_version(char* version)
{
    esp_http_client_config_t config = {
        .url = OTA_VERSION_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
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