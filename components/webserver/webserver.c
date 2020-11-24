#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <time.h>
#include "sdkconfig.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "http_parser.h"
#include "esp_interface.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_ota_ops.h"
#include "mbedtls/base64.h"

#include "webserver.h"
#include "evse.h"
#include "board_config.h"
#include "nvs_utils.h"

#define SCRATCH_BUFSIZE             1024

#define MAX_JSON_SIZE               (200*1024) // 200 KB
#define MAX_JSON_SIZE_STR           "200KB"

#define NVS_WEB_USER                "web_user"
#define NVS_WEB_PASSWORD            "web_password"

static const char *TAG = "webserver";

static char user[32];
static char password[32];

static bool autorize_req(httpd_req_t *req)
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
            if (mbedtls_base64_decode((unsigned char*) authorization, sizeof(authorization), &olen, (unsigned char*) authorization_hdr,
                    strlen(authorization_hdr)) == 0) {
                authorization[olen] = '\0';

                char req_user[32] = "";
                char req_password[32] = "";

                char *saveptr;
                char *token = strtok_r(authorization, ":", &saveptr);
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

static void restart_timer_callback(void *arg)
{
    esp_restart();
}

static void restart_timer_arm()
{
    // @formatter:off
        const esp_timer_create_args_t restart_timer_args = {
                .callback = &restart_timer_callback,
                .name = "restart"
        };
// @formatter:on
    esp_timer_handle_t restart_timer;
    esp_timer_create(&restart_timer_args, &restart_timer);
    esp_timer_start_once(restart_timer, 1000000);
}

static cJSON* read_request_json(httpd_req_t *req)
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

    char *body = (char*) malloc(sizeof(char) * total_len);
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

    cJSON *root = cJSON_Parse(body);

    free((void*) body);

    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not valid JSON");
        return NULL;
    }

    return root;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void send_response_chunhed(httpd_req_t *req, const uint8_t start[], const uint8_t end[])
{
    int remaining = end - start;
    while (remaining > 0) {
        httpd_resp_send_chunk(req, (const char*) (end - remaining), MIN(remaining, SCRATCH_BUFSIZE));
        remaining -= SCRATCH_BUFSIZE;
    }

    httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t favicon_ico_get_handler(httpd_req_t *req)
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

static esp_err_t index_html_get_handler(httpd_req_t *req)
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

static esp_err_t settings_html_get_handler(httpd_req_t *req)
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

static esp_err_t management_html_get_handler(httpd_req_t *req)
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

static esp_err_t about_html_get_handler(httpd_req_t *req)
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

static esp_err_t jquery_js_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        extern const uint8_t jquery_min_js_start[] asm("_binary_jquery_min_js_start");
        extern const uint8_t jquery_min_js_end[] asm("_binary_jquery_min_js_end");

        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        send_response_chunhed(req, jquery_min_js_start, jquery_min_js_end);
    }
    return ESP_OK;
}

static esp_err_t boostrap_js_get_handler(httpd_req_t *req)
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

static esp_err_t boostrap_css_get_handler(httpd_req_t *req)
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

static esp_err_t info_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "uptime", clock() / CLOCKS_PER_SEC);
        const esp_app_desc_t *app_desc = esp_ota_get_app_description();
        cJSON_AddStringToObject(root, "appVersion", app_desc->version);
        cJSON_AddStringToObject(root, "appDate", app_desc->date);
        cJSON_AddStringToObject(root, "appTime", app_desc->time);
        cJSON_AddStringToObject(root, "idfVersion", app_desc->idf_ver);
        cJSON_AddStringToObject(root, "chip", CONFIG_IDF_TARGET);
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        cJSON_AddNumberToObject(root, "chipCores", chip_info.cores);
        cJSON_AddNumberToObject(root, "chipRevision", chip_info.revision);
        uint8_t mac[6];
        char str[32];
        esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
        sprintf(str, MACSTR, MAC2STR(mac));
        cJSON_AddStringToObject(root, "mac", str);

        esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
        sprintf(str, MACSTR, MAC2STR(mac));
        cJSON_AddStringToObject(root, "macAp", str);

        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
        esp_ip4addr_ntoa(&ip_info.ip, str, sizeof(str));
        cJSON_AddStringToObject(root, "ip", str);

        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
        esp_ip4addr_ntoa(&ip_info.ip, str, sizeof(str));
        cJSON_AddStringToObject(root, "ipAp", str);

        const char *json = cJSON_Print(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);

        free((void*) json);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "chargingCurrent", evse_get_chaging_current() / 10.0);

        uint8_t u8 = 0;
        nvs_get_u8(nvs, NVS_WIFI_ENABLED, &u8);
        cJSON_AddBoolToObject(root, "wifiEnabled", u8);

        char str[64];
        size_t len = sizeof(str);
        nvs_get_str(nvs, NVS_WIFI_SSID, str, &len);
        if (len > 0) {
            cJSON_AddStringToObject(root, "wifiSSID", str);
        }

        const char *json = cJSON_Print(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);

        free((void*) json);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t settings_post_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        cJSON *root = read_request_json(req);
        if (root != NULL) {
            double num_value;

            if (cJSON_IsNumber(cJSON_GetObjectItem(root, "chargingCurrent"))) {
                num_value = cJSON_GetObjectItem(root, "chargingCurrent")->valuedouble;
                evse_set_chaging_current(num_value * 10);
            }

            if (cJSON_IsBool(cJSON_GetObjectItem(root, "wifiEnabled"))) {
                nvs_set_u8(nvs, NVS_WIFI_ENABLED, cJSON_GetObjectItem(root, "wifiEnabled")->valueint);
            }
            if (cJSON_IsString(cJSON_GetObjectItem(root, "wifiSSID"))) {
                nvs_set_str(nvs, NVS_WIFI_SSID, cJSON_GetObjectItem(root, "wifiSSID")->valuestring);
            }
            if (cJSON_IsString(cJSON_GetObjectItem(root, "wifiPassword"))) {
                nvs_set_str(nvs, NVS_WIFI_PASSWORD, cJSON_GetObjectItem(root, "wifiPassword")->valuestring);
            }

            nvs_commit(nvs);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, "Settings updated");

            cJSON_Delete(root);
        }
    }
    return ESP_OK;
}

static esp_err_t config_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        cJSON *root = cJSON_CreateObject();

        cJSON_AddNumberToObject(root, "maxChargingCurrent", board_config.max_charging_current);
        cJSON_AddNumberToObject(root, "cableLock", board_config.cable_lock);
        cJSON_AddNumberToObject(root, "energyMeter", board_config.energy_meter);

        const char *json = cJSON_Print(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);

        free((void*) json);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t credentials_post_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        cJSON *root = read_request_json(req);
        if (root != NULL) {
            if (cJSON_IsString(cJSON_GetObjectItem(root, "user"))) {
                strcpy(user, cJSON_GetObjectItem(root, "user")->valuestring);
            } else {
                user[0] = '\0';
            }
            ESP_LOGI(TAG, "Set credentials user: %s", user);
            nvs_set_str(nvs, NVS_WEB_USER, user);

            if (cJSON_IsString(cJSON_GetObjectItem(root, "password"))) {
                strcpy(password, cJSON_GetObjectItem(root, "password")->valuestring);
            } else {
                password[0] = '\0';
            }
            nvs_set_str(nvs, NVS_WEB_PASSWORD, password);
            ESP_LOGI(TAG, "Set credentials password: %s", strlen(password) ? "****" : "<none>");

            nvs_commit(nvs);

            httpd_resp_set_type(req, "plain/text");
            httpd_resp_sendstr(req, "Credentials updated");

            cJSON_Delete(root);
        }
    }
    return ESP_OK;
}

static esp_err_t state_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "state", evse_get_state());
        cJSON_AddNumberToObject(root, "error", evse_get_error());
        cJSON_AddNumberToObject(root, "elapsed", evse_get_session_elapsed());
        cJSON_AddNumberToObject(root, "consumption", evse_get_session_consumption());
        cJSON_AddNumberToObject(root, "actualPower", evse_get_session_actual_power());
        const char *json = cJSON_Print(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);

        free((void*) json);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t restart_post_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        // TODO check if can restart

        restart_timer_arm();

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "Restart in one second");
    }
    return ESP_OK;
}

static esp_err_t flash_post_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        // TODO check if can restart

        esp_err_t err;
        esp_ota_handle_t update_handle = 0;
        const esp_partition_t *update_partition = NULL;
        int received = 0;
        int remaining = req->content_len;
        char buf[SCRATCH_BUFSIZE];
        bool image_header_was_checked = false;

        update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x", update_partition->subtype, update_partition->address);
        if (update_partition == NULL) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
            return ESP_FAIL;
        }

        while (remaining > 0) {
//             ESP_LOGI(TAG, "Remaining size : %d", remaining);

            if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
                ESP_LOGE(TAG, "File receive failed!");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive request");
                return ESP_FAIL;
            } else if (image_header_was_checked == false) {
                if (received > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                    esp_app_desc_t new_app_desc;
                    memcpy(&new_app_desc, &buf[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New firmware version: %s", new_app_desc.version);

                    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
                    if (strcmp(app_desc->project_name, new_app_desc.project_name) != 0) {
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
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA failed");
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "OTA begin succeeded");
            }

            err = esp_ota_write(update_handle, (const void*) buf, received);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "OTA write failed (%s)", esp_err_to_name(err));
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA failed");
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
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA failed");
            return ESP_FAIL;
        }

        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA set boot partition failed (%s)", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA failed");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Prepare to restart system!");
        restart_timer_arm();

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "File uploaded successfully");
    }
    return ESP_OK;
}

void webserver_init(void)
{
    size_t len;
    if (ESP_OK != nvs_get_str(nvs, NVS_WEB_USER, user, &len)) {
        user[0] = '\0';
    }
    if (ESP_OK != nvs_get_str(nvs, NVS_WEB_PASSWORD, password, &len)) {
        password[0] = '\0';
    }

    ESP_LOGI(TAG, "Web user: '%s'", user);

    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    //config.uri_match_fn = httpd_uri_match_wildcard;

// Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");

// @formatter:off
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

        httpd_uri_t config_get_uri = {
               .uri = "/api/v1/config",
               .method = HTTP_GET,
               .handler = config_get_handler
       };
       httpd_register_uri_handler(server, &config_get_uri);

        httpd_uri_t settings_get_uri = {
                .uri = "/api/v1/settings",
                .method = HTTP_GET,
                .handler = settings_get_handler
        };
        httpd_register_uri_handler(server, &settings_get_uri);

        httpd_uri_t settings_post_uri = {
                .uri = "/api/v1/settings",
                .method = HTTP_POST,
                .handler = settings_post_handler
        };
        httpd_register_uri_handler(server, &settings_post_uri);

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

        httpd_uri_t flash_post_uri = {
                .uri = "/api/v1/flash",
                .method = HTTP_POST,
                .handler = flash_post_handler
        };
        httpd_register_uri_handler(server, &flash_post_uri);

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

        httpd_uri_t jquery_js_get_uri = {
                .uri = "/jquery.min.js",
                .method = HTTP_GET,
                .handler = jquery_js_get_handler
        };
        httpd_register_uri_handler(server, &jquery_js_get_uri);

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
// @formatter:on
    }
}
