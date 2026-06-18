#include "http.h"

#include <esp_err.h>
#include <esp_log.h>
#include <mbedtls/base64.h>
#include <nvs.h>

#include "http_dav.h"
#include "http_rest.h"
#include "http_web.h"

#define NVS_NAMESPACE "rest"
#define NVS_USER      "user"
#define NVS_PASSWORD  "password"

static const char* TAG = "rest";

static nvs_handle_t s_nvs;

static char s_user[32];

static char s_password[32];

static httpd_handle_t s_server = NULL;

bool http_authorize_req(httpd_req_t* req)
{
    if (!strlen(s_user) && !strlen(s_password)) {
        // no authentication
        return true;
    } else if (httpd_req_get_hdr_value_len(req, "Authorization") > 0) {
        char authorization_hdr[128];
        char authorization[64];

        httpd_req_get_hdr_value_str(req, "Authorization", authorization_hdr, sizeof(authorization_hdr));
        sscanf(authorization_hdr, "Basic %s", authorization_hdr);

        if (strlen(authorization_hdr) > 0) {
            size_t olen;
            if (mbedtls_base64_decode((unsigned char*)authorization, sizeof(authorization), &olen, (unsigned char*)authorization_hdr, strlen(authorization_hdr)) == 0) {
                authorization[olen] = '\0';

                char req_user[32] = "";
                char req_password[32] = "";

                char* saveptr;
                char* token = strtok_r(authorization, ":", &saveptr);
                if (token != NULL) {
                    strlcpy(req_user, token, sizeof(req_user));

                    token = strtok_r(NULL, ":", &saveptr);
                    if (token != NULL) {
                        strlcpy(req_password, token, sizeof(req_password));
                    }
                }

                if (strcmp(s_user, req_user) == 0 && strcmp(s_password, req_password) == 0) {
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

void http_set_credentials(const char* user, const char* password)
{
    strlcpy(s_user, user, sizeof(s_user));
    ESP_LOGI(TAG, "Set credentials user: %s", s_user);
    nvs_set_str(s_nvs, NVS_USER, s_user);

    strlcpy(s_password, password, sizeof(s_password));
    nvs_set_str(s_nvs, NVS_PASSWORD, s_password);
    ESP_LOGI(TAG, "Set credentials password: %s", strlen(s_password) ? "****" : "<none>");

    nvs_commit(s_nvs);
}

// #include "lwip/sockets.h"
// static int sess_count = 0;

// static void sess_on_close(httpd_handle_t hd, int sockfd)
// {
//     ESP_LOGW(TAG, "close %d (%d)", sockfd, --sess_count);
//     close(sockfd);
// }

// static esp_err_t sess_on_open(httpd_handle_t hd, int sockfd)
// {
//     ESP_LOGW(TAG, "open %d (%d)", sockfd, ++sess_count);
//     return ESP_OK;
// }

void http_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs));

    size_t len;
    if (ESP_OK != nvs_get_str(s_nvs, NVS_USER, s_user, &len)) {
        s_user[0] = '\0';
    }
    if (ESP_OK != nvs_get_str(s_nvs, NVS_PASSWORD, s_password, &len)) {
        s_password[0] = '\0';
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 5 * 1024;  // default 4096 not enought for OTA esp_https_ota
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = http_rest_handlers_count() + http_dav_handlers_count() + http_web_handlers_count();
    // Close the least-recently-used connection when the socket pool is full so a
    // new client can always get in. Without this, lingering/keep-alive
    // connections eventually fill all max_open_sockets slots and the server stops
    // accepting (UI unreachable while ping still works) until the link is reset.
    config.lru_purge_enable = true;
    // config.max_open_sockets = 3;
    // config.open_fn = sess_on_open;
    // config.close_fn = sess_on_close;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&s_server, &config));

    // ESP_LOGI(TAG, "Credentials user / password: %s / %s", user, password);

    http_rest_add_handlers(s_server);
    http_dav_add_handlers(s_server);
    http_web_add_handlers(s_server);
}
