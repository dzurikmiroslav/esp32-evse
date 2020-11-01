#include <string.h>
#include <stdlib.h>

#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "mbedtls/base64.h"

#include "webserver.h"
#include "storage.h"
#include "evse.h"

#define BUFSIZE                     (1024)

#define DEFAULT_USER                "admin"
#define DEFAULT_PASSWD              "admin"

#define NVS_WEB_USER                "web_user"
#define NVS_WEB_PASSWD              "web_passwd"

static const char *TAG = "webserver";

extern nvs_handle_t nvs;

static char user[32];
static char passwd[32];

static bool autorize_req(httpd_req_t *req)
{
    if (httpd_req_get_hdr_value_len(req, "Authorization") > 0) {
        char authorization_hdr[128];
        char authorization[64];

        httpd_req_get_hdr_value_str(req, "Authorization", authorization_hdr, sizeof(authorization_hdr));
        sscanf(authorization_hdr, "Basic %s", authorization_hdr);

        if (strlen(authorization_hdr) > 0) {
            size_t olen;
            if (mbedtls_base64_decode((unsigned char*) authorization, sizeof(authorization), &olen, (unsigned char*) authorization_hdr,
                    strlen(authorization_hdr)) == 0) {
                authorization[olen] = '\0';
                char *saveptr;
                char *auth_user = strtok_r(authorization, ":", &saveptr);
                char *auth_passwd = strtok_r(NULL, ":", &saveptr);
                if (strcmp(user, auth_user) == 0 && strcmp(passwd, auth_passwd) == 0) {
                    return true;
                } else {
                    ESP_LOGI(TAG, "Failed authorize user : %s", auth_user);
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

static esp_err_t index_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "text/html");

        extern const uint8_t index_html_start[] asm("_binary_index_html_start");
        extern const uint8_t index_html_end[] asm("_binary_index_html_end");
        const size_t index_html_size = (index_html_end - index_html_start);

        httpd_resp_send_chunk(req, (const char*) index_html_start, index_html_size);
    }
    return ESP_OK;
}

static esp_err_t info_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, HTTPD_TYPE_JSON);
        cJSON *root = cJSON_CreateObject();
        esp_chip_info_t chip_info;
        esp_chip_info(&chip_info);
        cJSON_AddNumberToObject(root, "uptime", clock()); // which is better?
        // cJSON_AddNumberToObject(root, "uptime", xTaskGetTickCount() / configTICK_RATE_HZ));
        cJSON_AddStringToObject(root, "idfVersion", IDF_VER);
        cJSON_AddStringToObject(root, "chip", CONFIG_IDF_TARGET);
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

        const char *sys_info = cJSON_Print(root);
        httpd_resp_sendstr(req, sys_info);
        free((void*) sys_info);
        cJSON_Delete(root);
    }
    return ESP_OK;
}

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, HTTPD_TYPE_JSON);
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "maxChargingCurrent", evse_get_max_chaging_current());
        cJSON_AddNumberToObject(root, "chargingCurrent", evse_get_chaging_current() / 10.0);
        cJSON_AddNumberToObject(root, "cableLock", evse_get_cable_lock());

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

        double num_value = cJSON_GetObjectItem(root, "maxChargingCurrent")->valuedouble;
        evse_set_max_chaging_current(num_value);

        num_value = cJSON_GetObjectItem(root, "chargingCurrent")->valuedouble;
        evse_set_chaging_current(num_value * 10);

        num_value = cJSON_GetObjectItem(root, "cableLock")->valuedouble;
        evse_set_cable_lock(num_value);

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

static esp_err_t state_get_handler(httpd_req_t *req)
{
    if (autorize_req(req)) {
        httpd_resp_set_type(req, "application/json");
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "state", evse_get_state());
        cJSON_AddNumberToObject(root, "error", evse_get_error());
        cJSON_AddNumberToObject(root, "l1Current", evse_get_l1_current());
        cJSON_AddNumberToObject(root, "l2Current", evse_get_l2_current());
        cJSON_AddNumberToObject(root, "l3Current", evse_get_l3_current());
        cJSON_AddNumberToObject(root, "l1Voltage", evse_get_l1_voltage());
        cJSON_AddNumberToObject(root, "l2Voltage", evse_get_l2_voltage());
        cJSON_AddNumberToObject(root, "l3Voltage", evse_get_l3_voltage());
        const char *sys_info = cJSON_Print(root);
        httpd_resp_sendstr(req, sys_info);
        free((void*) sys_info);
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
        strcpy(user, DEFAULT_USER);
    }
    if (ESP_OK != nvs_get_str(nvs, NVS_WEB_PASSWD, passwd, &len)) {
        strcpy(passwd, DEFAULT_PASSWD);
    }

    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
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

       httpd_uri_t index_get_uri = {
               .uri = "/index.html",
               .method = HTTP_GET,
               .handler = index_get_handler
       };
       httpd_register_uri_handler(server, &index_get_uri);

// @formatter:on
    }
}
