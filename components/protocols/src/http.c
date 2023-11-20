#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "mbedtls/base64.h"

#include "http.h"
#include "http_rest.h"
#include "http_web.h"
#include "http_dav.h"

#define MAX_JSON_SIZE           (50*1024) // 50 KB
#define MAX_JSON_SIZE_STR       "50KB"
#define MAX_OPEN_SOCKETS        5

#define NVS_NAMESPACE           "rest"
#define NVS_USER                "user"
#define NVS_PASSWORD            "password"

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

cJSON* http_read_request_json(httpd_req_t* req)
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
    config.max_uri_handlers = 17;
    // config.max_open_sockets = 3;
    // config.lru_purge_enable = true;
    // config.open_fn = sess_on_open;
    // config.close_fn = sess_on_close;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    // ESP_LOGI(TAG, "Credentials user / password: %s / %s", user, password);

    httpd_uri_t http_rest_partition_get_uri = {
        .uri = "/api/v1/partition/*",
        .method = HTTP_GET,
        .handler = http_rest_partition_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_partition_get_uri));

    httpd_uri_t http_rest_fs_file_get_uri = {
        .uri = "/api/v1/fs/*",
        .method = HTTP_GET,
        .handler = http_rest_fs_file_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_fs_file_get_uri));

    httpd_uri_t http_rest_fs_file_post_uri = {
        .uri = "/api/v1/fs/*",
        .method = HTTP_POST,
        .handler = http_rest_fs_file_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_fs_file_post_uri));

    httpd_uri_t http_rest_fs_file_delete_uri = {
        .uri = "/api/v1/fs/*",
        .method = HTTP_DELETE,
        .handler = http_rest_fs_file_delete_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_fs_file_delete_uri));

    httpd_uri_t http_rest_log_get_uri = {
        .uri = "/api/v1/log",
        .method = HTTP_GET,
        .handler = http_rest_log_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_log_get_uri));

    httpd_uri_t http_rest_script_output_get_uri = {
        .uri = "/api/v1/script/output",
        .method = HTTP_GET,
        .handler = http_rest_script_output_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_script_output_get_uri));

    httpd_uri_t http_rest_get_uri = {
       .uri = "/api/v1/*",
       .method = HTTP_GET,
       .handler = http_rest_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_get_uri));

    httpd_uri_t http_rest_state_post_uri = {
        .uri = "/api/v1/state/*",
        .method = HTTP_POST,
        .handler = http_rest_state_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_state_post_uri));

    httpd_uri_t http_rest_restart_post_uri = {
        .uri = "/api/v1/restart",
        .method = HTTP_POST,
        .handler = http_rest_restart_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_restart_post_uri));

    httpd_uri_t http_rest_firmware_update_post_uri = {
        .uri = "/api/v1/firmware/update",
        .method = HTTP_POST,
        .handler = http_rest_firmware_update_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_firmware_update_post_uri));

    httpd_uri_t http_rest_firmware_upload_post_uri = {
        .uri = "/api/v1/firmware/upload",
        .method = HTTP_POST,
        .handler = http_rest_firmware_upload_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_firmware_upload_post_uri));

    httpd_uri_t http_rest_script_reload_post_uri = {
        .uri = "/api/v1/script/reload",
        .method = HTTP_POST,
        .handler = http_rest_script_reload_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_script_reload_post_uri));

    httpd_uri_t http_rest_post_uri = {
       .uri = "/api/v1/*",
       .method = HTTP_POST,
       .handler = http_rest_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_rest_post_uri));

    httpd_uri_t http_dav_propfind_uri = {
        .uri = "/dav",
        .method = HTTP_PROPFIND,
        .handler = http_dav_propfind_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_dav_propfind_uri));

    httpd_uri_t http_dav_get_uri = {
        .uri = "/dav/*",
        .method = HTTP_GET,
        .handler = http_dav_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_dav_get_uri));

   httpd_uri_t http_dav_options_uri = {
        .uri = "/dav",
        .method = HTTP_OPTIONS,
        .handler = http_dav_options_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_dav_options_uri));

    httpd_uri_t http_web_get_uri = {
        .uri = "*",
        .method = HTTP_GET,
        .handler = http_web_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &http_web_get_uri));
}
