#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_client.h"

#include "include/ota.h"

static const char *TAG = "ota";

extern const char server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const char server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

ota_check_result_t ota_check(void)
{
    ota_check_result_t ota_result = { 0 };

    esp_http_client_config_t config = { .host = "api.github.com", .path = "/repos/dzurikmiroslav/esp32-evse/releases",
            .transport_type = HTTP_TRANSPORT_OVER_SSL, .event_handler = http_event_handler, .cert_pem = server_cert_pem_start };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {

    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    return ota_result;
}
