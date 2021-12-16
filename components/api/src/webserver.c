#include <sys/param.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "cJSON.h"
#include "mbedtls/base64.h"

#include "webserver.h"
#include "json_utils.h"
#include "ota_utils.h"
#include "timeout_utils.h"
#include "evse.h"

#define SCRATCH_BUFSIZE             1024

#define MAX_JSON_SIZE               (200*1024) // 200 KB
#define MAX_JSON_SIZE_STR           "200KB"

#define NVS_NAMESPACE               "evse_webserver"
#define NVS_USER                    "user"
#define NVS_PASSWORD                "password"

static const char* TAG = "webserver";

static nvs_handle_t nvs;

static char user[32];
static char password[32];

static bool autorize_req(httpd_req_t* req)
{
    if (!strlen(user) && !strlen(password)) {
        //no authentication
        return true;
    } else if (httpd_req_get_hdr_value_len(req, "Authorization") > 0) {
        char authorization_hdr[128];
        char authorization[64];

        httpd_req_get_hdr_value_str(req, "Authorization", authorization_hdr, sizeof(authorization_hdr));
        sscanf(authorization_hdr, "Basic %s", authorization_hdr);

        if (strlen(authorization_hdr) > 0) {
            size_t olen;
            if (mbedtls_base64_decode((unsigned char*)authorization, sizeof(authorization), &olen, (unsigned char*)authorization_hdr,
                strlen(authorization_hdr)) == 0) {
                authorization[olen] = '\0';

                char req_user[32] = "";
                char req_password[32] = "";

                char* saveptr;
                char* token = strtok_r(authorization, ":", &saveptr);
                if (token != NULL) {
                    strcpy(req_user, token);

                    token = strtok_r(NULL, ":", &saveptr);
                    if (token != NULL) {
                        strcpy(req_password, token);
                    }
                }

                if (strcmp(user, req_user) == 0 && strcmp(password, req_password) == 0) {
                    return true;
                } else {
                    ESP_LOGW(TAG, "Failed authorize user : %s", req_user);
                }
            }
        }
    }
    httpd_resp_set_status(req, "401");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Users\"");
    httpd_resp_sendstr(req, "Bad credentials");

    return false;
}

static cJSON* read_request_json(httpd_req_t* req)
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

static void write_reponse_json(httpd_req_t* req, cJSON* root)
{
    const char* json = cJSON_PrintUnformatted(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    free((void*)json);
}

static esp_err_t root_get_handler(httpd_req_t* req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void send_response_chunhed(httpd_req_t* req, const uint8_t start[], const uint8_t end[])
{
    int remaining = end - start;
    while (remaining > 0) {
        httpd_resp_send_chunk(req, (const char*)(end - remaining), MIN(remaining, SCRATCH_BUFSIZE));
        remaining -= SCRATCH_BUFSIZE;
    }

    httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t favicon_ico_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        extern const uint8_t _binary_favicon_ico_start[] asm("_binary_favicon_ico_start");
        extern const uint8_t _binary_favicon_ico_end[] asm("_binary_favicon_ico_end");

        httpd_resp_set_type(req, "image/x-icon");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, _binary_favicon_ico_start, _binary_favicon_ico_end);
    }
    return ESP_OK;
}

static esp_err_t index_html_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        extern const uint8_t index_html_start[] asm("_binary_index_html_start");
        extern const uint8_t index_html_end[] asm("_binary_index_html_end");

        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, index_html_start, index_html_end);
    }
    return ESP_OK;
}

static esp_err_t settings_html_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        extern const uint8_t settings_html_start[] asm("_binary_settings_html_start");
        extern const uint8_t settings_html_end[] asm("_binary_settings_html_end");

        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, settings_html_start, settings_html_end);
    }
    return ESP_OK;
}

static esp_err_t management_html_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        extern const uint8_t management_html_start[] asm("_binary_management_html_start");
        extern const uint8_t management_html_end[] asm("_binary_management_html_end");

        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, management_html_start, management_html_end);
    }
    return ESP_OK;
}

static esp_err_t about_html_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        extern const uint8_t about_html_start[] asm("_binary_about_html_start");
        extern const uint8_t about_html_end[] asm("_binary_about_html_end");

        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, about_html_start, about_html_end);
    }
    return ESP_OK;
}

static esp_err_t boostrap_js_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        extern const uint8_t bootstrap_bundle_min_js_start[] asm("_binary_bootstrap_bundle_min_js_start");
        extern const uint8_t bootstrap_bundle_min_js_end[] asm("_binary_bootstrap_bundle_min_js_end");

        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, bootstrap_bundle_min_js_start, bootstrap_bundle_min_js_end);
    }
    return ESP_OK;
}

static esp_err_t boostrap_css_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        extern const uint8_t bootstrap_min_css_start[] asm("_binary_bootstrap_min_css_start");
        extern const uint8_t bootstrap_min_css_end[] asm("_binary_bootstrap_min_css_end");

        httpd_resp_set_type(req, "text/css");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, bootstrap_min_css_start, bootstrap_min_css_end);
    }
    return ESP_OK;
}

static esp_err_t info_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = json_get_info();

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t config_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "evse", json_get_evse_config());
        cJSON_AddItemToObject(root, "wifi", json_get_wifi_config());
        cJSON_AddItemToObject(root, "mqtt", json_get_mqtt_config());
        cJSON_AddItemToObject(root, "tcpLogger", json_get_tcp_logger_config());

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t config_evse_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = json_get_evse_config();

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t config_wifi_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = json_get_wifi_config();

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t config_mqtt_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = json_get_mqtt_config();

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t config_tcp_logger_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = json_get_tcp_logger_config();

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t config_evse_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = read_request_json(req);
        if (root != NULL) {
            json_set_evse_config(root);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "Config updated");

            cJSON_Delete(root);
        }
    }
    return ESP_OK;
}

static esp_err_t config_wifi_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = read_request_json(req);
        if (root != NULL) {
            json_set_wifi_config(root);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "Config updated");

            cJSON_Delete(root);
        }
    }
    return ESP_OK;
}

static esp_err_t config_mqtt_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = read_request_json(req);
        if (root != NULL) {
            json_set_mqtt_config(root);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "Config updated");

            cJSON_Delete(root);
        }
    }
    return ESP_OK;
}

static esp_err_t config_tcp_logger_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = read_request_json(req);
        if (root != NULL) {
            json_set_tcp_logger_config(root);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "Config updated");

            cJSON_Delete(root);
        }
    }
    return ESP_OK;
}

static esp_err_t board_config_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = json_get_board_config();

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t credentials_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = read_request_json(req);
        if (root != NULL) {
            if (cJSON_IsString(cJSON_GetObjectItem(root, "user"))) {
                strcpy(user, cJSON_GetObjectItem(root, "user")->valuestring);
            } else {
                user[0] = '\0';
            }
            ESP_LOGI(TAG, "Set credentials user: %s", user);
            nvs_set_str(nvs, NVS_USER, user);

            if (cJSON_IsString(cJSON_GetObjectItem(root, "password"))) {
                strcpy(password, cJSON_GetObjectItem(root, "password")->valuestring);
            } else {
                password[0] = '\0';
            }
            nvs_set_str(nvs, NVS_PASSWORD, password);
            ESP_LOGI(TAG, "Set credentials password: %s", strlen(password) ? "****" : "<none>");

            nvs_commit(nvs);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "Credentials updated");

            cJSON_Delete(root);
        }
    }
    return ESP_OK;
}

static esp_err_t state_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = json_get_state();

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t restart_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/plain");

        evse_disable(EVSE_DISABLE_BIT_SYSTEM);
        httpd_resp_sendstr(req, "Restart in one second");
        timeout_restart();
    }
    return ESP_OK;
}

static esp_err_t firmware_check_update_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = cJSON_CreateObject();

        char avl_version[32];
        if (ota_get_available_version(avl_version) == ESP_OK) {
            const esp_app_desc_t* app_desc = esp_ota_get_app_description();

            cJSON_AddStringToObject(root, "available", avl_version);
            cJSON_AddStringToObject(root, "current", app_desc->version);
            cJSON_AddBoolToObject(root, "newer", ota_is_newer_version(app_desc->version, avl_version));

            ESP_LOGI(TAG, "Running firmware version: %s", app_desc->version);
            ESP_LOGI(TAG, "Available firmware version: %s", avl_version);
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot be fetch latest version info");
            cJSON_Delete(root);
            return ESP_FAIL;
        }

        write_reponse_json(req, root);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t firmware_update_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        evse_disable(EVSE_DISABLE_BIT_SYSTEM);

        httpd_resp_set_type(req, "text/plain");

        char avl_version[32];
        if (ota_get_available_version(avl_version) == ESP_OK) {
            const esp_app_desc_t* app_desc = esp_ota_get_app_description();

            if (ota_is_newer_version(app_desc->version, avl_version)) {
                extern const char server_cert_pem_start[] asm("_binary_ca_cert_pem_start");

                esp_http_client_config_t config = {
                    .url = OTA_FIRMWARE_URL,
                    .cert_pem = server_cert_pem_start
                };

                esp_err_t err = esp_https_ota(&config);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "OTA upgrade done");
                    timeout_restart();
                } else {
                    ESP_LOGE(TAG, "OTA failed (%s)", esp_err_to_name(err));
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upgrade failed");
                    evse_enable(EVSE_DISABLE_BIT_SYSTEM);
                    return ESP_FAIL;
                }
            }
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot be fetch latest version info");
            evse_enable(EVSE_DISABLE_BIT_SYSTEM);
            return ESP_FAIL;
        }

        httpd_resp_sendstr(req, "Firmware upgraded successfully");
    }
    return ESP_OK;
}

static esp_err_t firmware_upload_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        evse_disable(EVSE_DISABLE_BIT_SYSTEM);

        httpd_resp_set_type(req, "text/plain");

        esp_err_t err;
        esp_ota_handle_t update_handle = 0;
        const esp_partition_t* update_partition = NULL;
        int received = 0;
        int remaining = req->content_len;
        char buf[SCRATCH_BUFSIZE];
        bool image_header_was_checked = false;

        update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);
        if (update_partition == NULL) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
            evse_enable(EVSE_DISABLE_BIT_SYSTEM);
            return ESP_FAIL;
        }

        while (remaining > 0) {
            if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
                ESP_LOGE(TAG, "File receive failed");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive request");
                evse_enable(EVSE_DISABLE_BIT_SYSTEM);
                return ESP_FAIL;
            } else if (image_header_was_checked == false) {
                if (received > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                    esp_app_desc_t new_app_desc;
                    memcpy(&new_app_desc, &buf[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New firmware version: %s", new_app_desc.version);

                    const esp_app_desc_t* app_desc = esp_ota_get_app_description();
                    if (strcmp(app_desc->project_name, new_app_desc.project_name) != 0)
                    {
                        ESP_LOGE(TAG, "Received firmware is not %s", app_desc->project_name);
                        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware file");
                        evse_enable(EVSE_DISABLE_BIT_SYSTEM);
                        return ESP_FAIL;
                    }
                } else {
                    ESP_LOGE(TAG, "Received package is not fit length");
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware file");
                    evse_enable(EVSE_DISABLE_BIT_SYSTEM);
                    return ESP_FAIL;
                }

                image_header_was_checked = true;

                err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "OTA begin failed (%s)", esp_err_to_name(err));
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
                    evse_enable(EVSE_DISABLE_BIT_SYSTEM);
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "OTA begin succeeded");
            }

            err = esp_ota_write(update_handle, (const void*)buf, received);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OTA write failed (%s)", esp_err_to_name(err));
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
                evse_enable(EVSE_DISABLE_BIT_SYSTEM);
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
            evse_enable(EVSE_DISABLE_BIT_SYSTEM);
            return ESP_FAIL;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA set boot partition failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
            evse_enable(EVSE_DISABLE_BIT_SYSTEM);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Prepare to restart system!");
        timeout_restart();

        httpd_resp_sendstr(req, "File uploaded successfully");
    }
    return ESP_OK;
}

void webserver_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    size_t len;
    if (ESP_OK != nvs_get_str(nvs, NVS_USER, user, &len)) {
        user[0] = '\0';
    }
    if (ESP_OK != nvs_get_str(nvs, NVS_PASSWORD, password, &len)) {
        password[0] = '\0';
    }

    ESP_LOGI(TAG, "Web user: '%s'", user);
    ESP_LOGI(TAG, "Web password: '%s'", password); // TODO remove

    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 30;
    //config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");

        httpd_uri_t info_get_uri = {
            .uri = "/api/v1/info",
            .method = HTTP_GET,
            .handler = info_get_handler
        };
        httpd_register_uri_handler(server, &info_get_uri);

        httpd_uri_t state_get_uri = {
            .uri = "/api/v1/state",
            .method = HTTP_GET,
            .handler = state_get_handler
        };
        httpd_register_uri_handler(server, &state_get_uri);

        httpd_uri_t board_config_get_uri = {
            .uri = "/api/v1/boardConfig",
            .method = HTTP_GET,
            .handler = board_config_get_handler
        };
        httpd_register_uri_handler(server, &board_config_get_uri);

        httpd_uri_t config_get_uri = {
            .uri = "/api/v1/config",
            .method = HTTP_GET,
            .handler = config_get_handler
        };
        httpd_register_uri_handler(server, &config_get_uri);

        httpd_uri_t config_evse_get_uri = {
            .uri = "/api/v1/config/evse",
            .method = HTTP_GET,
            .handler = config_evse_get_handler
        };
        httpd_register_uri_handler(server, &config_evse_get_uri);

        httpd_uri_t config_wifi_get_uri = {
            .uri = "/api/v1/config/wifi",
            .method = HTTP_GET,
            .handler = config_wifi_get_handler
        };
        httpd_register_uri_handler(server, &config_wifi_get_uri);

        httpd_uri_t config_mqtt_get_uri = {
            .uri = "/api/v1/config/mqtt",
            .method = HTTP_GET,
            .handler = config_mqtt_get_handler
        };
        httpd_register_uri_handler(server, &config_mqtt_get_uri);

        httpd_uri_t config_tcp_logger_get_uri = {
            .uri = "/api/v1/config/tcpLogger",
            .method = HTTP_GET,
            .handler = config_tcp_logger_get_handler
        };
        httpd_register_uri_handler(server, &config_tcp_logger_get_uri);

        httpd_uri_t config_evse_post_uri = {
            .uri = "/api/v1/config/evse",
            .method = HTTP_POST,
            .handler = config_evse_post_handler
        };
        httpd_register_uri_handler(server, &config_evse_post_uri);

        httpd_uri_t config_wifi_post_uri = {
            .uri = "/api/v1/config/wifi",
            .method = HTTP_POST,
            .handler = config_wifi_post_handler
        };
        httpd_register_uri_handler(server, &config_wifi_post_uri);

        httpd_uri_t config_mqtt_post_uri = {
            .uri = "/api/v1/config/mqtt",
            .method = HTTP_POST,
            .handler = config_mqtt_post_handler
        };
        httpd_register_uri_handler(server, &config_mqtt_post_uri);

        httpd_uri_t config_tcp_logger_post_uri = {
            .uri = "/api/v1/config/tcpLogger",
            .method = HTTP_POST,
            .handler = config_tcp_logger_post_handler
        };
        httpd_register_uri_handler(server, &config_tcp_logger_post_uri);

        httpd_uri_t credentials_post_uri = {
            .uri = "/api/v1/credentials",
            .method = HTTP_POST,
            .handler = credentials_post_handler
        };
        httpd_register_uri_handler(server, &credentials_post_uri);

        httpd_uri_t restart_post_uri = {
            .uri = "/api/v1/restart",
            .method = HTTP_POST,
            .handler = restart_post_handler
        };
        httpd_register_uri_handler(server, &restart_post_uri);

        httpd_uri_t firmware_check_update_get_uri = {
            .uri = "/api/v1/firmware/checkUpdate",
            .method = HTTP_GET,
            .handler = firmware_check_update_get_handler
        };
        httpd_register_uri_handler(server, &firmware_check_update_get_uri);

        httpd_uri_t firmware_update_post_uri = {
            .uri = "/api/v1/firmware/update",
            .method = HTTP_POST,
            .handler = firmware_update_post_handler
        };
        httpd_register_uri_handler(server, &firmware_update_post_uri);

        httpd_uri_t firmware_upload_post_uri = {
            .uri = "/api/v1/firmware/upload",
            .method = HTTP_POST,
            .handler = firmware_upload_post_handler
        };
        httpd_register_uri_handler(server, &firmware_upload_post_uri);

        //web
        httpd_uri_t root_get_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler
        };
        httpd_register_uri_handler(server, &root_get_uri);

        httpd_uri_t favicon_ico_get_uri = {
            .uri = "/favicon.ico",
            .method = HTTP_GET,
            .handler = favicon_ico_get_handler
        };
        httpd_register_uri_handler(server, &favicon_ico_get_uri);

        httpd_uri_t index_html_get_uri = {
            .uri = "/index.html",
            .method = HTTP_GET,
            .handler = index_html_get_handler
        };
        httpd_register_uri_handler(server, &index_html_get_uri);

        httpd_uri_t settings_html_get_uri = {
            .uri = "/settings.html",
            .method = HTTP_GET,
            .handler = settings_html_get_handler
        };
        httpd_register_uri_handler(server, &settings_html_get_uri);

        httpd_uri_t management_html_get_uri = {
            .uri = "/management.html",
            .method = HTTP_GET,
            .handler = management_html_get_handler
        };
        httpd_register_uri_handler(server, &management_html_get_uri);

        httpd_uri_t about_html_get_uri = {
            .uri = "/about.html",
            .method = HTTP_GET,
            .handler = about_html_get_handler
        };
        httpd_register_uri_handler(server, &about_html_get_uri);

        httpd_uri_t bootstrap_css_get_uri = {
            .uri = "/bootstrap.min.css",
            .method = HTTP_GET,
            .handler = boostrap_css_get_handler
        };
        httpd_register_uri_handler(server, &bootstrap_css_get_uri);

        httpd_uri_t bootstrap_js_get_uri = {
            .uri = "/bootstrap.bundle.min.js",
            .method = HTTP_GET,
            .handler = boostrap_js_get_handler
        };
        httpd_register_uri_handler(server, &bootstrap_js_get_uri);
    }
}
