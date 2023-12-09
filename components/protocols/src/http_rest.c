#include <string.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "cJSON.h"

#include "http_rest.h"
#include "http.h"
#include "json.h"
#include "ota.h"
#include "timeout_utils.h"
#include "evse.h"
#include "script.h"
#include "logger.h"


#define REST_BASE_PATH           "/api/v1"
//#define REST_BASE_PATH_LEN       4
#define SCRATCH_BUFSIZE         1024
#define FILE_PATH_MAX           (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define MAX_JSON_SIZE           (50*1024) // 50 KB
#define MAX_JSON_SIZE_STR       "50KB"

static const char* TAG = "http_rest";


cJSON* read_request_json(httpd_req_t* req)
{
    char content_type[32];
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    if (strcmp(content_type, "application/json") != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not JSON body");
        return NULL;
    }

    int total_len = req->content_len;

    if (total_len > MAX_JSON_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON size must be less than " MAX_JSON_SIZE_STR "!");
        return NULL;
    }

    char* body = (char*)malloc(sizeof(char) * (total_len + 1));
    if (body == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
        return NULL;
    }

    int cur_len = 0;
    int received = 0;

    while (cur_len < total_len) {
        received = httpd_req_recv(req, body + cur_len, total_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed receive request");
            free((void*)body);
            return NULL;
        }
        cur_len += received;
    }
    body[total_len] = '\0';

    cJSON* root = cJSON_Parse(body);

    free((void*)body);

    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not valid JSON");
        return NULL;
    }

    return root;
}

static cJSON* firmware_check_update()
{
    cJSON* root = NULL;

    char avl_version[32];
    if (ota_get_available_version(avl_version) == ESP_OK) {
        const esp_app_desc_t* app_desc = esp_app_get_description();

        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "available", avl_version);
        cJSON_AddStringToObject(root, "current", app_desc->version);
        cJSON_AddBoolToObject(root, "newer", ota_is_newer_version(app_desc->version, avl_version));

        ESP_LOGI(TAG, "Running firmware version: %s", app_desc->version);
        ESP_LOGI(TAG, "Available firmware version: %s", avl_version);
    }

    return root;
}

static void set_credentials(cJSON* root)
{
    char* user;
    char* password;

    if (cJSON_IsString(cJSON_GetObjectItem(root, "user"))) {
        user = cJSON_GetObjectItem(root, "user")->valuestring;
    } else {
        user = "";
    }

    if (cJSON_IsString(cJSON_GetObjectItem(root, "password"))) {
        password = cJSON_GetObjectItem(root, "password")->valuestring;
    } else {
        password = "";
    }

    http_set_credentials(user, password);
}

esp_err_t get_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        cJSON* root = NULL;

        if (strcmp(req->uri, REST_BASE_PATH"/info") == 0) {
            root = json_get_info();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/boardConfig") == 0) {
            root = json_get_board_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/state") == 0) {
            root = json_get_state();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config") == 0) {
            root = cJSON_CreateObject();
            cJSON_AddItemToObject(root, "evse", json_get_evse_config());
            cJSON_AddItemToObject(root, "wifi", json_get_wifi_config());
            cJSON_AddItemToObject(root, "mqtt", json_get_mqtt_config());
            cJSON_AddItemToObject(root, "serial", json_get_serial_config());
            cJSON_AddItemToObject(root, "modbus", json_get_modbus_config());
            cJSON_AddItemToObject(root, "script", json_get_script_config());
            cJSON_AddItemToObject(root, "time", json_get_time_config());
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/evse") == 0) {
            root = json_get_evse_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/wifi") == 0) {
            root = json_get_wifi_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/wifi/scan") == 0) {
            root = json_get_wifi_scan();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/mqtt") == 0) {
            root = json_get_mqtt_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/serial") == 0) {
            root = json_get_serial_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/modbus") == 0) {
            root = json_get_modbus_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/script") == 0) {
            root = json_get_script_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/time") == 0) {
            root = json_get_time_config();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/firmware/checkUpdate") == 0) {
            root = firmware_check_update();
            if (root == NULL) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot be fetch latest version info");
                return ESP_FAIL;
            }
        }

        if (root == NULL) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "This URI does not exist");
            return ESP_FAIL;
        } else {
            const char* json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, json);

            free((void*)json);
        }

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t post_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        cJSON* root = read_request_json(req);

        esp_err_t ret = ESP_FAIL;

        if (root == NULL) {
            return ESP_FAIL;
        }

        if (strcmp(req->uri, REST_BASE_PATH"/config/evse") == 0) {
            ret = json_set_evse_config(root);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/wifi") == 0) {
            ret = json_set_wifi_config(root, true);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/mqtt") == 0) {
            ret = json_set_mqtt_config(root);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/serial") == 0) {
            ret = json_set_serial_config(root);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/modbus") == 0) {
            ret = json_set_modbus_config(root);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/script") == 0) {
            ret = json_set_script_config(root);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/config/time") == 0) {
            ret = json_set_time_config(root);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/credentials") == 0) {
            set_credentials(root);
            ret = ESP_OK;
        }

        cJSON_Delete(root);

        if (ret == ESP_OK) {
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "OK");

            return ESP_OK;
        } else {
            httpd_resp_send_err(req, ret == ESP_ERR_INVALID_STATE ? HTTPD_500_INTERNAL_SERVER_ERROR : HTTPD_400_BAD_REQUEST, NULL);

            return ESP_FAIL;
        }
    } else {
        return ESP_FAIL;
    }
}

esp_err_t restart_post_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        timeout_restart();

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t state_post_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        httpd_resp_set_type(req, "text/plain");

        if (strcmp(req->uri, REST_BASE_PATH"/state/authorize") == 0) {
            evse_authorize();
        }
        if (strcmp(req->uri, REST_BASE_PATH"/state/enable") == 0) {
            evse_set_enabled(true);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/state/disable") == 0) {
            evse_set_enabled(false);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/state/available") == 0) {
            evse_set_available(true);
        }
        if (strcmp(req->uri, REST_BASE_PATH"/state/unavailable") == 0) {
            evse_set_available(false);
        }

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t firmware_update_post_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        char avl_version[32];
        if (ota_get_available_version(avl_version) == ESP_OK) {
            const esp_app_desc_t* app_desc = esp_app_get_description();

            if (ota_is_newer_version(app_desc->version, avl_version)) {
                extern const char server_cert_pem_start[] asm("_binary_ca_cert_pem_start");

                esp_http_client_config_t http_config = {
                    .url = OTA_FIRMWARE_URL CONFIG_IDF_TARGET "-evse.bin",
                    .cert_pem = server_cert_pem_start
                };

                esp_https_ota_config_t config = {
                    .http_config = &http_config
                };

                esp_err_t err = esp_https_ota(&config);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "OTA upgrade done");
                    timeout_restart();
                } else {
                    ESP_LOGE(TAG, "OTA failed (%s)", esp_err_to_name(err));
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upgrade failed");
                    return ESP_FAIL;
                }
            }
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot be fetch latest version info");
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t firmware_upload_post_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        esp_err_t err;
        esp_ota_handle_t update_handle = 0;
        const esp_partition_t* update_partition = NULL;
        int received = 0;
        int remaining = req->content_len;
        char buf[SCRATCH_BUFSIZE];
        bool image_header_was_checked = false;

        update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%lx", update_partition->subtype, update_partition->address);
        if (update_partition == NULL) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
            return ESP_FAIL;
        }

        while (remaining > 0) {
            if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
                ESP_LOGE(TAG, "File receive failed");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
                return ESP_FAIL;
            } else if (image_header_was_checked == false) {
                if (received > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                    esp_app_desc_t new_app_desc;
                    memcpy(&new_app_desc, &buf[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New firmware version: %s", new_app_desc.version);

                    const esp_app_desc_t* app_desc = esp_app_get_description();
                    if (strcmp(app_desc->project_name, new_app_desc.project_name) != 0)
                    {
                        ESP_LOGE(TAG, "Received firmware is not %s", app_desc->project_name);
                        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware file");
                        return ESP_FAIL;
                    }
                } else {
                    ESP_LOGE(TAG, "Received package is not fit length");
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware file");
                    return ESP_FAIL;
                }

                image_header_was_checked = true;

                err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "OTA begin failed (%s)", esp_err_to_name(err));
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "OTA begin succeeded");
            }

            err = esp_ota_write(update_handle, (const void*)buf, received);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OTA write failed (%s)", esp_err_to_name(err));
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
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
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
            return ESP_FAIL;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA set boot partition failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Prepare to restart system!");
        timeout_restart();

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t script_reload_post_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        script_reload();

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t log_get_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
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
    } else {
        return ESP_FAIL;
    }
}

esp_err_t script_output_get_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
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
    } else {
        return ESP_FAIL;
    }
}

size_t http_rest_handlers_count(void)
{
    return 9;
}

void http_rest_add_handlers(httpd_handle_t server)
{
    httpd_uri_t log_get_uri = {
        .uri = REST_BASE_PATH"/log",
        .method = HTTP_GET,
        .handler = log_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &log_get_uri));

    httpd_uri_t script_output_get_uri = {
        .uri = REST_BASE_PATH"/script/output",
        .method = HTTP_GET,
        .handler = script_output_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &script_output_get_uri));

    httpd_uri_t get_uri = {
       .uri = REST_BASE_PATH"/*",
       .method = HTTP_GET,
       .handler = get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));

    httpd_uri_t state_post_uri = {
        .uri = REST_BASE_PATH"/state/*",
        .method = HTTP_POST,
        .handler = state_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &state_post_uri));

    httpd_uri_t restart_post_uri = {
        .uri = REST_BASE_PATH"/restart",
        .method = HTTP_POST,
        .handler = restart_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restart_post_uri));

    httpd_uri_t firmware_update_post_uri = {
        .uri = REST_BASE_PATH"/firmware/update",
        .method = HTTP_POST,
        .handler = firmware_update_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &firmware_update_post_uri));

    httpd_uri_t firmware_upload_post_uri = {
        .uri = REST_BASE_PATH"/firmware/upload",
        .method = HTTP_POST,
        .handler = firmware_upload_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &firmware_upload_post_uri));

    httpd_uri_t script_reload_post_uri = {
        .uri = REST_BASE_PATH"/script/reload",
        .method = HTTP_POST,
        .handler = script_reload_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &script_reload_post_uri));

    httpd_uri_t post_uri = {
       .uri = REST_BASE_PATH"/*",
       .method = HTTP_POST,
       .handler = post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &post_uri));
}