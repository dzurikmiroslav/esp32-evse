#include "http_rest.h"

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_err.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>

#include "energy_meter.h"
#include "evse.h"
#include "http.h"
#include "http_json.h"
#include "logger.h"
#include "ota.h"
#include "script.h"
#include "serial_nextion.h"

#define REST_BASE_PATH    "/api/v1"
#define SCRATCH_BUFSIZE   1024
#define MAX_JSON_SIZE     (50 * 1024)  // 50 KB
#define MAX_JSON_SIZE_STR "50KB"

static const char* TAG = "http_rest";

typedef enum {
    URI_NONE = -1,
    //
    URI_STATE_AUTHORIZE,
    URI_STATE_ENABLED,
    URI_STATE_AVAILABLE,
    URI_STATE_CHARGING_CURRENT,
    URI_STATE_CONSUMPTION_LIMIT,
    URI_STATE_CHARGING_TIME_LIMIT,
    URI_STATE_UNDER_POWER_LIMIT,
    URI_STATE_RESET_TOTAL_CONSUMPTION,
    URI_STATE,
    URI_CONFIG_EVSE,
    URI_CONFIG_WIFI,
    URI_CONFIG_SERIAL,
    URI_CONFIG_MODBUS,
    URI_CONFIG_SCRIPT,
    URI_CONFIG_SCHEDULER,
    URI_CONFIG,
    URI_WIFI_SCAN,
    URI_SCRIPT_OUTPUT,
    URI_SCRIPT_RELOAD,
    URI_SCRIPT_COMPONENTS_ID,
    URI_SCRIPT_COMPONENTS,
    URI_FIRMWARE_CHANNELS,
    URI_FIRMWARE_CHANNEL,
    URI_FIRMWARE_CHECK_UPDATE,
    URI_FIRMWARE_UPDATE,
    URI_FIRMWARE_UPLOAD,
    URI_NEXTION_INFO,
    URI_NEXTION_UPLOAD,
    URI_LOG,
    URI_INFO,
    URI_BOARD_CONFIG,
    URI_TIME,
    URI_RESTART,
    URI_CREDENTIALS,
    //
    URI_MAX
} uri_t;

// keep order first more specific uri
static const char* uris[] = {
    "/state/authorize",
    "/state/enabled",
    "/state/available",
    "/state/charging-current",
    "/state/consumption-limit",
    "/state/charging-time-limit",
    "/state/under-power-limit",
    "/state/reset-total-consumption",
    "/state",
    "/config/evse",
    "/config/wifi",
    "/config/serial",
    "/config/modbus",
    "/config/script",
    "/config/scheduler",
    "/config",
    "/wifi/scan",
    "/script/output",
    "/script/reload",
    "/script/components/",
    "/script/components",
    "/firmware/channels",
    "/firmware/channel",
    "/firmware/check-update",
    "/firmware/update",
    "/firmware/upload",
    "/nextion/info",
    "/nextion/upload",
    "/log",
    "/info",
    "/board-config",
    "/time",
    "/restart",
    "/credentials",
};

#define uri_full_length(uri) (sizeof(REST_BASE_PATH) - 1 + strlen(uris[uri]))

static uri_t get_uri(const char* uri)
{
    // skip REST_BASE_PATH, httpd_register_uri_handler will filter it
    const char* sub_uri = uri + sizeof(REST_BASE_PATH) - 1;
    for (uri_t i = URI_NONE + 1; i < URI_MAX; i++) {
        if (!strncmp(sub_uri, uris[i], strlen(uris[i]))) return i;
    }
    return URI_NONE;
}

cJSON* read_request_json(httpd_req_t* req)
{
    char content_type[32];
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    if (strcmp(content_type, "application/json") != 0) {
        httpd_resp_send_custom_err(req, "415 Unsupported Media Type", "Not JSON body");
        return NULL;
    }

    int total_len = req->content_len;

    if (total_len > MAX_JSON_SIZE) {
        httpd_resp_send_custom_err(req, "413 Content Too Large", "JSON size must be less than " MAX_JSON_SIZE_STR "!");
        return NULL;
    }

    char* body = (char*)malloc(sizeof(char) * (total_len + 1));
    if (body == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        httpd_resp_send_custom_err(req, "512 Failed To Allocate Memory", "Failed to allocate memory");
        return NULL;
    }

    int cur_len = 0;
    int received = 0;

    while (cur_len < total_len) {
        received = httpd_req_recv(req, body + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_custom_err(req, "513 Failed To Receive Request", "Failed to receive request");
            free((void*)body);
            return NULL;
        }
        cur_len += received;
    }
    body[total_len] = '\0';

    cJSON* root = cJSON_Parse(body);

    free((void*)body);

    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);
        return NULL;
    }

    return root;
}

esp_err_t handle_json_response(httpd_req_t* req, cJSON* json)
{
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
        return ESP_FAIL;
    } else {
        const char* json_str = cJSON_PrintUnformatted(json);
        cJSON_Delete(json);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json_str);

        free((void*)json_str);
    }

    return ESP_OK;
}

esp_err_t write_response_ok(httpd_req_t* req, esp_err_t ret)
{
    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");
    } else {
        switch (ret) {
        case ESP_ERR_NOT_FOUND:
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
            break;
        case ESP_ERR_INVALID_STATE:
        case ESP_ERR_INVALID_ARG:
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);
            break;
        default:
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        }
    }

    return ret;
}

esp_err_t handle_not_found(httpd_req_t* req)
{
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);

    return ESP_FAIL;
}

static esp_err_t handle_log(httpd_req_t* req)
{
    uint16_t count = logger_count();
    char count_str[16];
    itoa(count, count_str, 10);
    httpd_resp_set_hdr(req, "X-Count", count_str);

    uint16_t index = 0;
    char buf[24];
    char param[16];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if (httpd_query_key_value(buf, "index", param, sizeof(param)) == ESP_OK) {
            index = atoi(param);
        }
    }

    httpd_resp_set_type(req, "text/plain");
    char* line;
    uint16_t line_len;
    while (logger_read(&index, &line, &line_len) && index <= count) {
        if (httpd_resp_send_chunk(req, line, line_len) != ESP_OK) {
            ESP_LOGE(TAG, "Sending failed");
            httpd_resp_sendstr_chunk(req, NULL);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
            return ESP_FAIL;
        }
    }

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t handle_script_output(httpd_req_t* req)
{
    uint16_t count = script_output_count();
    char count_str[16];
    itoa(count, count_str, 10);
    httpd_resp_set_hdr(req, "X-Count", count_str);

    uint16_t index = 0;
    char buf[24];
    char param[16];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if (httpd_query_key_value(buf, "index", param, sizeof(param)) == ESP_OK) {
            index = atoi(param);
        }
    }

    httpd_resp_set_type(req, "text/plain");
    char* line;
    uint16_t line_len;
    while (script_output_read(&index, &line, &line_len) && index <= count) {
        if (httpd_resp_send_chunk(req, line, line_len) != ESP_OK) {
            ESP_LOGE(TAG, "Sending failed");
            httpd_resp_sendstr_chunk(req, NULL);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
            return ESP_FAIL;
        }
    }

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

esp_err_t handle_json_request(httpd_req_t* req, esp_err_t (*action)(cJSON*))
{
    cJSON* json = read_request_json(req);
    if (!json) {
        return ESP_FAIL;
    }

    esp_err_t ret = action(json);

    cJSON_free(json);

    return write_response_ok(req, ret);
}

esp_err_t handle_str_json_request(httpd_req_t* req, const char* str, esp_err_t (*action)(const char*, cJSON*))
{
    cJSON* json = read_request_json(req);
    if (!json) {
        return ESP_FAIL;
    }

    esp_err_t ret = action(str, json);

    cJSON_free(json);

    return write_response_ok(req, ret);
}

esp_err_t handle_void_request(httpd_req_t* req, void (*action)(void))
{
    action();

    return write_response_ok(req, ESP_OK);
}

static esp_err_t handle_firmware_update(httpd_req_t* req)
{
    char* version;
    char* path;
    if (ota_get_available(&version, &path) == ESP_OK) {
        const esp_app_desc_t* app_desc = esp_app_get_description();

        bool not_match = strcmp(app_desc->version, version) != 0;
        free((void*)version);

        if (not_match) {
            esp_http_client_config_t http_config = {
                .url = path,
                .crt_bundle_attach = esp_crt_bundle_attach,
            };
            esp_https_ota_config_t config = {
                .http_config = &http_config,
            };

            esp_err_t err = esp_https_ota(&config);
            free((void*)path);

            if (err == ESP_OK) {
                ESP_LOGI(TAG, "OTA upgrade done");
                schedule_restart();
            } else {
                ESP_LOGE(TAG, "OTA failed (%s)", esp_err_to_name(err));
                httpd_resp_send_custom_err(req, "521 Failed To Upgrade Firmware", "Failed to upgrade firmware");
                return ESP_FAIL;
            }
        }
    } else {
        httpd_resp_send_custom_err(req, "520 Cannot Fetch Latest Version Info", "Cannot fetch latest version info");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

static esp_err_t handle_firmware_upload(httpd_req_t* req)
{
    esp_err_t err;
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t* update_partition = NULL;
    int received = 0;
    size_t remaining = req->content_len;
    char buf[SCRATCH_BUFSIZE];
    bool image_header_was_checked = false;

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx", update_partition->subtype, update_partition->address);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition");
        httpd_resp_send_custom_err(req, "521 Failed To Upgrade Firmware", "Failed to upgrade firmware");
        return ESP_FAIL;
    }

    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            ESP_LOGE(TAG, "File receive failed");
            httpd_resp_send_custom_err(req, "522 Failed To Receive Firmware", "Failed to receive firmware");
            return ESP_FAIL;
        } else if (image_header_was_checked == false) {
            if (received > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                esp_app_desc_t new_app_desc;
                memcpy(&new_app_desc, &buf[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                ESP_LOGI(TAG, "New firmware version: %s", new_app_desc.version);

                const esp_app_desc_t* app_desc = esp_app_get_description();
                if (strcmp(app_desc->project_name, new_app_desc.project_name) != 0) {
                    ESP_LOGE(TAG, "Received firmware is not %s", app_desc->project_name);
                    httpd_resp_send_custom_err(req, "523 Invalid Firmware File", "Invalid firmware file");
                    return ESP_FAIL;
                }
            } else {
                ESP_LOGE(TAG, "Received package is not fit length");
                httpd_resp_send_custom_err(req, "523 Invalid Firmware File", "Invalid firmware file");
                return ESP_FAIL;
            }

            image_header_was_checked = true;

            err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OTA begin failed (%s)", esp_err_to_name(err));
                httpd_resp_send_custom_err(req, "521 Failed To Upgrade Firmware", "Failed to upgrade firmware");
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "OTA begin succeeded");
        }

        err = esp_ota_write(update_handle, (const void*)buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed (%s)", esp_err_to_name(err));
            httpd_resp_send_custom_err(req, "521 Failed To Upgrade Firmware", "Failed to upgrade firmware");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        ESP_LOGE(TAG, "OTA end failed (%s)!", esp_err_to_name(err));
        httpd_resp_send_custom_err(req, "521 Failed To Upgrade Firmware", "Failed to upgrade firmware");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA set boot partition failed (%s)", esp_err_to_name(err));
        httpd_resp_send_custom_err(req, "521 Failed To Upgrade Firmware", "Failed to upgrade firmware");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Prepare to restart system!");
    schedule_restart();

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");

    return ESP_OK;
}

static esp_err_t handle_nextion_upload(httpd_req_t* req)
{
    esp_err_t err;

    size_t remaining = req->content_len;

    char* buf = (char*)malloc(sizeof(char) * SERIAL_NEXTION_UPLOAD_BATCH_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        httpd_resp_send_custom_err(req, "512 Failed To Allocate Memory", "Failed to allocate memory");
        return ESP_FAIL;
    }

    uint32_t baud_rate = 921600;
    char param[16];
    size_t skip_remaining, skip_to;
    if (httpd_req_get_url_query_str(req, buf, SERIAL_NEXTION_UPLOAD_BATCH_SIZE) == ESP_OK) {
        if (httpd_query_key_value(buf, "baud-rate", param, sizeof(param)) == ESP_OK) {
            baud_rate = atoi(param);
        }
    }

    if (serial_nextion_upload_begin(remaining, baud_rate) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to begin write program");
        httpd_resp_send_custom_err(req, "531 Failed To Write Program", "Failed to write program");
        free((void*)buf);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int received = 0;
        do {
            int ret = httpd_req_recv(req, &buf[received], MIN(remaining, SERIAL_NEXTION_UPLOAD_BATCH_SIZE) - received);
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            } else if (ret <= 0) {
                ESP_LOGE(TAG, "File receive failed");
                httpd_resp_send_custom_err(req, "532 Failed To Receive Program", "Failed to receive program");
                free((void*)buf);
                serial_nextion_upload_end();
                return ESP_FAIL;
            }

            received += ret;
        } while (received < MIN(remaining, SERIAL_NEXTION_UPLOAD_BATCH_SIZE));
        remaining -= received;

        err = serial_nextion_upload_write(buf, received, &skip_to);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write program");
            httpd_resp_send_custom_err(req, "531 Failed To Write Program", "Failed to write program");
            free((void*)buf);
            return ESP_FAIL;
        }

        if (skip_to > 0) {
            skip_remaining = skip_to - (req->content_len - remaining);
            while (skip_remaining > 0) {
                if ((received = httpd_req_recv(req, buf, MIN(skip_remaining, SERIAL_NEXTION_UPLOAD_BATCH_SIZE))) <= 0) {
                    ESP_LOGE(TAG, "File receive failed");
                    httpd_resp_send_custom_err(req, "532 Failed To Receive Program", "Failed to receive program");
                    free((void*)buf);
                    serial_nextion_upload_end();
                    return ESP_FAIL;
                }
                skip_remaining -= received;
                remaining -= received;
            }
        }
    }

    serial_nextion_upload_end();

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");

    free((void*)buf);

    return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        switch (get_uri(req->uri)) {
        case URI_STATE:
            return handle_json_response(req, http_json_get_state());
        case URI_CONFIG_EVSE:
            return handle_json_response(req, http_json_get_config_evse());
        case URI_CONFIG_WIFI:
            return handle_json_response(req, http_json_get_config_wifi());
        case URI_CONFIG_SERIAL:
            return handle_json_response(req, http_json_get_config_serial());
        case URI_CONFIG_MODBUS:
            return handle_json_response(req, http_json_get_config_modbus());
        case URI_CONFIG_SCRIPT:
            return handle_json_response(req, http_json_get_config_script());
        case URI_CONFIG_SCHEDULER:
            return handle_json_response(req, http_json_get_config_scheduler());
        case URI_CONFIG:
            return handle_json_response(req, http_json_get_config());
        case URI_WIFI_SCAN:
            return handle_json_response(req, http_json_get_wifi_scan());
        case URI_SCRIPT_OUTPUT:
            return handle_script_output(req);
        case URI_SCRIPT_COMPONENTS:
            return handle_json_response(req, http_json_get_script_components());
        case URI_SCRIPT_COMPONENTS_ID:
            return handle_json_response(req, http_json_get_script_component_config(req->uri + uri_full_length(URI_SCRIPT_COMPONENTS_ID)));
        case URI_FIRMWARE_CHANNELS:
            return handle_json_response(req, http_json_firmware_channels());
        case URI_FIRMWARE_CHANNEL:
            return handle_json_response(req, http_json_firmware_channel());
        case URI_FIRMWARE_CHECK_UPDATE:
            return handle_json_response(req, http_json_firmware_check_update());
        case URI_NEXTION_INFO:
            return handle_json_response(req, http_json_get_nextion_info());
        case URI_LOG:
            return handle_log(req);
        case URI_INFO:
            return handle_json_response(req, http_json_get_info());
        case URI_BOARD_CONFIG:
            return handle_json_response(req, http_json_get_board_config());
        case URI_TIME:
            return handle_json_response(req, http_json_get_time());
        default:
            return handle_not_found(req);
        }
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t post_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        switch (get_uri(req->uri)) {
        case URI_STATE:
            return handle_json_request(req, http_json_set_state);
        case URI_STATE_ENABLED:
            return handle_json_request(req, http_json_set_state_enabled);
        case URI_STATE_AVAILABLE:
            return handle_json_request(req, http_json_set_state_available);
        case URI_STATE_CHARGING_CURRENT:
            return handle_json_request(req, http_json_set_state_charging_current);
        case URI_STATE_CONSUMPTION_LIMIT:
            return handle_json_request(req, http_json_set_state_consumption_limit);
        case URI_STATE_CHARGING_TIME_LIMIT:
            return handle_json_request(req, http_json_set_state_charging_time_limit);
        case URI_STATE_UNDER_POWER_LIMIT:
            return handle_json_request(req, http_json_set_state_under_power_limit);
        case URI_STATE_AUTHORIZE:
            return handle_void_request(req, evse_authorize);
        case URI_STATE_RESET_TOTAL_CONSUMPTION:
            return handle_void_request(req, energy_meter_reset_total_consumption);
        case URI_CONFIG_EVSE:
            return handle_json_request(req, http_json_set_config_evse);
        case URI_CONFIG_WIFI:
            return handle_json_request(req, http_json_set_config_wifi);
        case URI_CONFIG_SERIAL:
            return handle_json_request(req, http_json_set_config_serial);
        case URI_CONFIG_MODBUS:
            return handle_json_request(req, http_json_set_config_modbus);
        case URI_CONFIG_SCRIPT:
            return handle_json_request(req, http_json_set_config_script);
        case URI_CONFIG_SCHEDULER:
            return handle_json_request(req, http_json_set_config_scheduler);
        case URI_SCRIPT_RELOAD:
            return handle_void_request(req, script_reload);
        case URI_SCRIPT_COMPONENTS_ID:
            return handle_str_json_request(req, req->uri + uri_full_length(URI_SCRIPT_COMPONENTS_ID), http_json_set_script_component_config);
        case URI_FIRMWARE_CHANNEL:
            return handle_json_request(req, http_json_set_firmware_channel);
        case URI_FIRMWARE_UPDATE:
            return handle_firmware_update(req);
        case URI_FIRMWARE_UPLOAD:
            return handle_firmware_upload(req);
        case URI_NEXTION_UPLOAD:
            return handle_nextion_upload(req);
        case URI_RESTART:
            return handle_void_request(req, schedule_restart);
        case URI_TIME:
            return handle_json_request(req, http_json_set_time);
        case URI_CREDENTIALS:
            return handle_json_request(req, http_json_set_credentials);
        default:
            return handle_not_found(req);
        }
    } else {
        return ESP_FAIL;
    }
}

size_t http_rest_handlers_count(void)
{
    return 2;
}

void http_rest_add_handlers(httpd_handle_t server)
{
    httpd_uri_t get_uri = {
        .uri = REST_BASE_PATH "/*",
        .method = HTTP_GET,
        .handler = get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));

    httpd_uri_t post_uri = {
        .uri = REST_BASE_PATH "/*",
        .method = HTTP_POST,
        .handler = post_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &post_uri));
}