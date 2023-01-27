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

#include "rest.h"
#include "json.h"
#include "ota.h"
#include "timeout_utils.h"
#include "evse.h"

#define SCRATCH_BUFSIZE     1024

#define MAX_JSON_SIZE       (200*1024) // 200 KB
#define MAX_JSON_SIZE_STR   "200KB"

#define NVS_NAMESPACE       "rest"
#define NVS_USER            "user"
#define NVS_PASSWORD        "password"

static const char* TAG = "rest";

extern const char web_favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const char web_favicon_ico_end[] asm("_binary_favicon_ico_end");
extern const char web_index_html_start[] asm("_binary_index_html_start");
extern const char web_index_html_end[] asm("_binary_index_html_end");
extern const char web_settings_html_start[] asm("_binary_settings_html_start");
extern const char web_settings_html_end[] asm("_binary_settings_html_end");
extern const char web_management_html_start[] asm("_binary_management_html_start");
extern const char web_management_html_end[] asm("_binary_management_html_end");
extern const char web_about_html_start[] asm("_binary_about_html_start");
extern const char web_about_html_end[] asm("_binary_about_html_end");
extern const char web_bootstrap_bundle_min_js_start[] asm("_binary_bootstrap_bundle_min_js_start");
extern const char web_bootstrap_bundle_min_js_end[] asm("_binary_bootstrap_bundle_min_js_end");
extern const char web_bootstrap_min_css_start[] asm("_binary_bootstrap_min_css_start");
extern const char web_bootstrap_min_css_end[] asm("_binary_bootstrap_min_css_end");

static nvs_handle_t nvs;

static char user[32];
static char password[32];

static httpd_handle_t server = NULL;

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

void set_credentials(cJSON* root)
{
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
}

static esp_err_t root_get_handler(httpd_req_t* req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t web_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        const char* type;
        const char* buf_start;
        const char* buf_end;

        if (strcmp(req->uri, "/favicon.ico") == 0) {
            type = "image/x-icon";
            buf_start = web_favicon_ico_start;
            buf_end = web_favicon_ico_end;
        }
        if (strcmp(req->uri, "/index.html") == 0) {
            type = "text/html";
            buf_start = web_index_html_start;
            buf_end = web_index_html_end;
        }
        if (strcmp(req->uri, "/settings.html") == 0) {
            type = "text/html";
            buf_start = web_settings_html_start;
            buf_end = web_settings_html_end;
        }
        if (strcmp(req->uri, "/management.html") == 0) {
            type = "text/html";
            buf_start = web_management_html_start;
            buf_end = web_management_html_end;
        }
        if (strcmp(req->uri, "/about.html") == 0) {
            type = "text/html";
            buf_start = web_about_html_start;
            buf_end = web_about_html_end;
        }
        if (strcmp(req->uri, "/bootstrap.bundle.min.js") == 0) {
            type = "application/javascript";
            buf_start = web_bootstrap_bundle_min_js_start;
            buf_end = web_bootstrap_bundle_min_js_end;
        }
        if (strcmp(req->uri, "/bootstrap.min.css") == 0) {
            type = "text/css";
            buf_start = web_bootstrap_min_css_start;
            buf_end = web_bootstrap_min_css_end;
        }

        if (type == NULL) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "This URI does not exist");
            return ESP_FAIL;
        } else {
            httpd_resp_set_type(req, type);
            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            httpd_resp_send(req, (const char*)(buf_start), buf_end - buf_start);
        }

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t json_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = NULL;

        if (strcmp(req->uri, "/api/v1/info") == 0) {
            root = json_get_info();
        }
        if (strcmp(req->uri, "/api/v1/state") == 0) {
            root = json_get_state();
        }
        if (strcmp(req->uri, "/api/v1/boardConfig") == 0) {
            root = json_get_board_config();
        }
        if (strcmp(req->uri, "/api/v1/state") == 0) {
            root = json_get_state();
        }
        if (strcmp(req->uri, "/api/v1/config") == 0) {
            root = cJSON_CreateObject();
            cJSON_AddItemToObject(root, "evse", json_get_evse_config());
            cJSON_AddItemToObject(root, "wifi", json_get_wifi_config());
            cJSON_AddItemToObject(root, "mqtt", json_get_mqtt_config());
            cJSON_AddItemToObject(root, "tcpLogger", json_get_tcp_logger_config());
            cJSON_AddItemToObject(root, "serial", json_get_serial_config());
            cJSON_AddItemToObject(root, "modbus", json_get_modbus_config());
        }
        if (strcmp(req->uri, "/api/v1/config/evse") == 0) {
            root = json_get_evse_config();
        }
        if (strcmp(req->uri, "/api/v1/config/wifi") == 0) {
            root = json_get_wifi_config();
        }
        if (strcmp(req->uri, "/api/v1/config/wifi/scan") == 0) {
            root = json_get_wifi_scan();
        }
        if (strcmp(req->uri, "/api/v1/config/mqtt") == 0) {
            root = json_get_mqtt_config();
        }
        if (strcmp(req->uri, "/api/v1/config/tcpLogger") == 0) {
            root = json_get_tcp_logger_config();
        }
        if (strcmp(req->uri, "/api/v1/config/serial") == 0) {
            root = json_get_serial_config();
        }
        if (strcmp(req->uri, "/api/v1/config/modbus") == 0) {
            root = json_get_modbus_config();
        }
        if (strcmp(req->uri, "/api/v1/firmware/checkUpdate") == 0) {
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

static esp_err_t json_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        cJSON* root = read_request_json(req);
        const char* res_msg;
        esp_err_t ret = ESP_OK;

        if (root == NULL) {
            return ESP_FAIL;
        }

        if (strcmp(req->uri, "/api/v1/config/evse") == 0) {
            ret = json_set_evse_config(root);
            res_msg = "Config updated";
        }
        if (strcmp(req->uri, "/api/v1/config/wifi") == 0) {
            ret = json_set_wifi_config(root, true);
            res_msg = "Config updated";
        }
        if (strcmp(req->uri, "/api/v1/config/mqtt") == 0) {
            ret = json_set_mqtt_config(root);
            res_msg = "Config updated";
        }
        if (strcmp(req->uri, "/api/v1/config/tcpLogger") == 0) {
            ret = json_set_tcp_logger_config(root);
            res_msg = "Config updated";
        }
        if (strcmp(req->uri, "/api/v1/config/serial") == 0) {
            ret = json_set_serial_config(root);
            res_msg = "Config updated";
        }
        if (strcmp(req->uri, "/api/v1/config/modbus") == 0) {
            ret = json_set_modbus_config(root);
            res_msg = "Config updated";
        }
        if (strcmp(req->uri, "/api/v1/credentials") == 0) {
            set_credentials(root);
            res_msg = "Credentials updated";
        }

        cJSON_Delete(root);

        httpd_resp_set_type(req, "text/plain");

        if (ret == ESP_OK) {
            httpd_resp_sendstr(req, res_msg);

            return ESP_OK;
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, NULL);

            return ESP_FAIL;
        }
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t restart_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/plain");

        evse_set_available(false);
        httpd_resp_sendstr(req, "Restart in one second");
        timeout_restart();

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t state_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/plain");
        const char* res_msg;

        if (strcmp(req->uri, "/api/v1/state/authorize") == 0) {
            evse_authorize();
            res_msg = "Auhtorized";
        }
        if (strcmp(req->uri, "/api/v1/state/enable") == 0) {
            evse_set_enabled(true);
            res_msg = "Enabled";
        }
        if (strcmp(req->uri, "/api/v1/state/disable") == 0) {
            evse_set_enabled(false);
            res_msg = "Disabled";
        }

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, res_msg);

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t firmware_update_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        evse_set_available(false);

        httpd_resp_set_type(req, "text/plain");

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
                    evse_set_available(true);
                    return ESP_FAIL;
                }
            }
        } else {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot be fetch latest version info");
            evse_set_available(true);
            return ESP_FAIL;
        }

        httpd_resp_sendstr(req, "Firmware upgraded successfully");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t firmware_upload_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        evse_set_available(false);

        httpd_resp_set_type(req, "text/plain");

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
            evse_set_available(true);
            return ESP_FAIL;
        }

        while (remaining > 0) {
            if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
                ESP_LOGE(TAG, "File receive failed");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
                evse_set_available(true);
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
                        evse_set_available(true);
                        return ESP_FAIL;
                    }
                } else {
                    ESP_LOGE(TAG, "Received package is not fit length");
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid firmware file");
                    evse_set_available(true);
                    return ESP_FAIL;
                }

                image_header_was_checked = true;

                err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "OTA begin failed (%s)", esp_err_to_name(err));
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
                    evse_set_available(true);
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "OTA begin succeeded");
            }

            err = esp_ota_write(update_handle, (const void*)buf, received);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OTA write failed (%s)", esp_err_to_name(err));
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
                evse_set_available(true);
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
            evse_set_available(true);
            return ESP_FAIL;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA set boot partition failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware upload failed");
            evse_set_available(true);
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Prepare to restart system!");
        timeout_restart();

        httpd_resp_sendstr(req, "Firmware uploaded successfully");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t board_config_raw_get_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        FILE* fd = fopen("/cfg/board.cfg", "r");
        if (!fd) {
            ESP_LOGE(TAG, "Failed to open board config");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open board config");
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"board.cfg\"");

        char buf[SCRATCH_BUFSIZE];
        size_t len;
        do {
            len = fread(buf, sizeof(char), SCRATCH_BUFSIZE, fd);
            if (len > 0) {
                if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                    fclose(fd);
                    ESP_LOGE(TAG, "File sending failed!");
                    httpd_resp_sendstr_chunk(req, NULL);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                    return ESP_FAIL;
                }
            }
        } while (len != 0);

        fclose(fd);

        httpd_resp_send_chunk(req, NULL, 0);

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t board_config_raw_post_handler(httpd_req_t* req)
{
    if (autorize_req(req)) {
        FILE* fd = fopen("/cfg/board.cfg", "w");
        if (!fd) {
            ESP_LOGE(TAG, "Failed to open board config");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open board config");
            return ESP_FAIL;
        }

        int received = 0;
        int remaining = req->content_len;
        char buf[SCRATCH_BUFSIZE];

        while (remaining > 0) {
            ESP_LOGW(TAG, "write remain %d", remaining);

            if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
                fclose(fd);

                ESP_LOGE(TAG, "File receive failed");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
                return ESP_FAIL;
            }

            if (received != fwrite(buf, sizeof(char), received, fd)) {
                fclose(fd);

                ESP_LOGE(TAG, "File write failed!");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
                return ESP_FAIL;
            }

            remaining -= received;
        }

        fclose(fd);

        httpd_resp_sendstr(req, "File uploaded successfully");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

void rest_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    size_t len;
    if (ESP_OK != nvs_get_str(nvs, NVS_USER, user, &len)) {
        user[0] = '\0';
    }
    if (ESP_OK != nvs_get_str(nvs, NVS_PASSWORD, password, &len)) {
        password[0] = '\0';
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 10;
    config.max_open_sockets = 3;
    //config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t board_config_raw_get_uri = {
        .uri = "/api/v1/boardConfig/raw",
        .method = HTTP_GET,
        .handler = board_config_raw_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &board_config_raw_get_uri));

    httpd_uri_t board_config_raw_post_uri = {
        .uri = "/api/v1/boardConfig/raw",
        .method = HTTP_POST,
        .handler = board_config_raw_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &board_config_raw_post_uri));

    httpd_uri_t json_get_uri = {
       .uri = "/api/v1/*",
       .method = HTTP_GET,
       .handler = json_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &json_get_uri));

    httpd_uri_t state_post_uri = {
        .uri = "/api/v1/state/*",
        .method = HTTP_POST,
        .handler = state_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &state_post_uri));

    httpd_uri_t restart_post_uri = {
        .uri = "/api/v1/restart",
        .method = HTTP_POST,
        .handler = restart_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restart_post_uri));

    httpd_uri_t firmware_update_post_uri = {
        .uri = "/api/v1/firmware/update",
        .method = HTTP_POST,
        .handler = firmware_update_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &firmware_update_post_uri));

    httpd_uri_t firmware_upload_post_uri = {
        .uri = "/api/v1/firmware/upload",
        .method = HTTP_POST,
        .handler = firmware_upload_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &firmware_upload_post_uri));

    httpd_uri_t json_post_uri = {
       .uri = "/api/v1/*",
       .method = HTTP_POST,
       .handler = json_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &json_post_uri));

    httpd_uri_t root_get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_get_uri));

    httpd_uri_t web_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = web_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &web_get_uri));
}
