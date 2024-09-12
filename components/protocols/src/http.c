#include "http.h"

#include <esp_err.h>
#include <esp_log.h>
#include <mbedtls/base64.h>
#include <nvs.h>

#include "http_dav.h"
#include "http_rest.h"
#include "http_web.h"

#define MAX_OPEN_SOCKETS 5

#define NVS_NAMESPACE "rest"
#define NVS_USER      "user"
#define NVS_PASSWORD  "password"

static const char* TAG = "rest";

static nvs_handle_t nvs;

static char user[32];
static char password[32];

static httpd_handle_t server = NULL;

bool http_authorize_req(httpd_req_t* req)
{
    if (!strlen(user) && !strlen(password)) {
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

void http_set_credentials(const char* _user, const char* _password)
{
    strcpy(user, _user);
    ESP_LOGI(TAG, "Set credentials user: %s", user);
    nvs_set_str(nvs, NVS_USER, user);

    strcpy(password, _password);
    nvs_set_str(nvs, NVS_PASSWORD, password);
    ESP_LOGI(TAG, "Set credentials password: %s", strlen(password) ? "****" : "<none>");

    nvs_commit(nvs);
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
    config.max_uri_handlers = http_rest_handlers_count() + http_dav_handlers_count() + http_web_handlers_count();
    // config.max_open_sockets = 3;
    // config.lru_purge_enable = true;
    // config.open_fn = sess_on_open;
    // config.close_fn = sess_on_close;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    // ESP_LOGI(TAG, "Credentials user / password: %s / %s", user, password);

    http_rest_add_handlers(server);
    http_dav_add_handlers(server);
    http_web_add_handlers(server);
}
