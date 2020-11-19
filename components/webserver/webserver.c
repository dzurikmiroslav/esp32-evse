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

#define BUFSIZE                     (1024)

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
            if (mbedtls_base64_decode((unsigned char*) authorization, sizeof(authorization), &olen,
                    (unsigned char*) authorization_hdr, strlen(authorization_hdr)) == 0) {
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
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Users\"");
    httpd_resp_send(req, NULL, 0);

    return false;
}

static void restart_timer_callback(void *arg)
{
    esp_restart();
}

static esp_err_t read_buf(httpd_req_t *req, char *buf)
{
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;
    if (total_len >= BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    return ESP_OK;
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
        httpd_resp_send_chunk(req, (const char*) (end - remaining), MIN(remaining, BUFSIZE));
        remaining -= BUFSIZE;
    }

    httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t favicon_ico_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "image/x-icon");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t _binary_favicon_ico_start[] asm("_binary_favicon_ico_start");
        extern const uint8_t _binary_favicon_ico_end[] asm("_binary_favicon_ico_end");

        send_response_chunhed(req, _binary_favicon_ico_start, _binary_favicon_ico_end);
    }
    return ESP_OK;
}

static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t index_html_start[] asm("_binary_index_html_start");
        extern const uint8_t index_html_end[] asm("_binary_index_html_end");

        send_response_chunhed(req, index_html_start, index_html_end);
    }
    return ESP_OK;
}

static esp_err_t settings_html_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t settings_html_start[] asm("_binary_settings_html_start");
        extern const uint8_t settings_html_end[] asm("_binary_settings_html_end");

        send_response_chunhed(req, settings_html_start, settings_html_end);
    }
    return ESP_OK;
}

static esp_err_t management_html_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t management_html_start[] asm("_binary_management_html_start");
        extern const uint8_t management_html_end[] asm("_binary_management_html_end");

        send_response_chunhed(req, management_html_start, management_html_end);
    }
    return ESP_OK;
}

static esp_err_t about_html_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t about_html_start[] asm("_binary_about_html_start");
        extern const uint8_t about_html_end[] asm("_binary_about_html_end");

        send_response_chunhed(req, about_html_start, about_html_end);
    }
    return ESP_OK;
}

static esp_err_t jquery_js_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t jquery_min_js_start[] asm("_binary_jquery_min_js_start");
        extern const uint8_t jquery_min_js_end[] asm("_binary_jquery_min_js_end");

        send_response_chunhed(req, jquery_min_js_start, jquery_min_js_end);
    }
    return ESP_OK;
}

static esp_err_t boostrap_js_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t bootstrap_bundle_min_js_start[] asm("_binary_bootstrap_bundle_min_js_start");
        extern const uint8_t bootstrap_bundle_min_js_end[] asm("_binary_bootstrap_bundle_min_js_end");

        send_response_chunhed(req, bootstrap_bundle_min_js_start, bootstrap_bundle_min_js_end);
    }
    return ESP_OK;
}

static esp_err_t boostrap_css_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

        extern const uint8_t bootstrap_min_css_start[] asm("_binary_bootstrap_min_css_start");
        extern const uint8_t bootstrap_min_css_end[] asm("_binary_bootstrap_min_css_end");

        send_response_chunhed(req, bootstrap_min_css_start, bootstrap_min_css_end);
    }
    return ESP_OK;
}

static esp_err_t info_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, HTTPD_TYPE_JSON);
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
        httpd_resp_sendstr(req, json);
        free((void*) json);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, HTTPD_TYPE_JSON);
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
        const char *sys_info = cJSON_Print(root);
        httpd_resp_sendstr(req, sys_info);
        free((void*) sys_info);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t settings_post_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        char buf[BUFSIZE];
        esp_err_t err = read_buf(req, buf);
        if (err != ESP_OK) {
            return err;
        }
        cJSON *root = cJSON_Parse(buf);

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

        cJSON_Delete(root);
        httpd_resp_set_status(req, "201");
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t config_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, HTTPD_TYPE_JSON);
        cJSON *root = cJSON_CreateObject();

        cJSON_AddNumberToObject(root, "maxChargingCurrent", board_config.max_charging_current);
        cJSON_AddNumberToObject(root, "cableLock", board_config.cable_lock);
        cJSON_AddNumberToObject(root, "energyMeter", board_config.energy_meter);

        const char *json = cJSON_Print(root);
        httpd_resp_sendstr(req, json);
        free((void*) json);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t credentials_post_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        char buf[BUFSIZE];
        esp_err_t err = read_buf(req, buf);
        if (err != ESP_OK) {
            return err;
        }
        cJSON *root = cJSON_Parse(buf);

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

        cJSON_Delete(root);
        httpd_resp_set_status(req, "201");
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}

static esp_err_t state_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "application/json");
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "state", evse_get_state());
        cJSON_AddNumberToObject(root, "error", evse_get_error());
        cJSON_AddNumberToObject(root, "elapsed", evse_get_session_elapsed());
        cJSON_AddNumberToObject(root, "consumption", evse_get_session_consumption());
        cJSON_AddNumberToObject(root, "actualPower", evse_get_session_actual_power());
        const char *json = cJSON_Print(root);
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

        char buf[BUFSIZE];
        esp_err_t err = read_buf(req, buf);
        if (err != ESP_OK) {
            return err;
        }
        cJSON *root = cJSON_Parse(buf);

        if (cJSON_IsNumber(cJSON_GetObjectItem(root, "time"))) {
            double num_value = cJSON_GetObjectItem(root, "time")->valuedouble;
            ESP_LOGI(TAG, "Restart in %d ms", (int )num_value);

// @formatter:off
    const esp_timer_create_args_t restart_timer_args = {
            .callback = &restart_timer_callback,
            .name = "restart"
    };
// @formatter:on
            esp_timer_handle_t restart_timer;
            ESP_ERROR_CHECK(esp_timer_create(&restart_timer_args, &restart_timer));
            ESP_ERROR_CHECK(esp_timer_start_once(restart_timer, num_value * 1000));

            httpd_resp_set_status(req, "201");
        } else {
            httpd_resp_set_status(req, "400");
        }

        cJSON_Delete(root);
        httpd_resp_send(req, NULL, 0);
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
