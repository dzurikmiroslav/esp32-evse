#include <string.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "cJSON.h"
#include "mbedtls/base64.h"

#include "rest.h"
#include "json.h"
#include "ota.h"
#include "timeout_utils.h"
#include "evse.h"
#include "script.h"
#include "logger.h"
#include "web_archive.h"

#define SCRATCH_BUFSIZE         1024

#define MAX_JSON_SIZE           (50*1024) // 50 KB
#define MAX_JSON_SIZE_STR       "50KB"
#define FILE_PATH_MAX           (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define MAX_OPEN_SOCKETS        5

#define NVS_NAMESPACE           "rest"
#define NVS_USER                "user"
#define NVS_PASSWORD            "password"

static const char* TAG = "rest";

static nvs_handle_t nvs;

static char user[32];
static char password[32];

static httpd_handle_t server = NULL;

static bool authorize_req(httpd_req_t* req)
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

static esp_err_t web_get_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        char* file_name = req->uri + 1;
        char* file_suffix = strrchr(req->uri, '.');
        strcat(file_name, ".gz");

        char* file_data;
        uint32_t file_size = web_archive_find(file_name, &file_data);
        if (file_size == 0) {
            //fallback to index.html
            file_size = web_archive_find("index.html.gz", &file_data);
            file_suffix = ".html.gz";
        }

        if (file_size > 0) {
            if (strcmp(file_suffix, ".html.gz") == 0) {
                httpd_resp_set_type(req, "text/html");
            } else if (strcmp(file_suffix, ".css.gz") == 0) {
                httpd_resp_set_type(req, "text/css");
            } else if (strcmp(file_suffix, ".js.gz") == 0) {
                httpd_resp_set_type(req, "application/javascript");
            } else if (strcmp(file_suffix, ".json.gz") == 0) {
                httpd_resp_set_type(req, "application/json");
            } else if (strcmp(file_suffix, ".svg.gz") == 0) {
                httpd_resp_set_type(req, "application/svg+xml");
            } else if (strcmp(file_suffix, ".ico.gz") == 0) {
                httpd_resp_set_type(req, "image/x-icon");
            }

            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            httpd_resp_send(req, file_data, file_size);
        } else {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
            return ESP_FAIL;
        }

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t json_get_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        cJSON* root = NULL;

        if (strcmp(req->uri, "/api/v1/info") == 0) {
            root = json_get_info();
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
            cJSON_AddItemToObject(root, "serial", json_get_serial_config());
            cJSON_AddItemToObject(root, "modbus", json_get_modbus_config());
            cJSON_AddItemToObject(root, "script", json_get_script_config());
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
        if (strcmp(req->uri, "/api/v1/config/serial") == 0) {
            root = json_get_serial_config();
        }
        if (strcmp(req->uri, "/api/v1/config/modbus") == 0) {
            root = json_get_modbus_config();
        }
        if (strcmp(req->uri, "/api/v1/config/script") == 0) {
            root = json_get_script_config();
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
    if (authorize_req(req)) {
        cJSON* root = read_request_json(req);

        esp_err_t ret = ESP_OK;

        if (root == NULL) {
            return ESP_FAIL;
        }

        if (strcmp(req->uri, "/api/v1/config/evse") == 0) {
            ret = json_set_evse_config(root);
        }
        if (strcmp(req->uri, "/api/v1/config/wifi") == 0) {
            ret = json_set_wifi_config(root, true);
        }
        if (strcmp(req->uri, "/api/v1/config/mqtt") == 0) {
            ret = json_set_mqtt_config(root);
        }
        if (strcmp(req->uri, "/api/v1/config/serial") == 0) {
            ret = json_set_serial_config(root);
        }
        if (strcmp(req->uri, "/api/v1/config/modbus") == 0) {
            ret = json_set_modbus_config(root);
        }
        if (strcmp(req->uri, "/api/v1/config/script") == 0) {
            ret = json_set_script_config(root);
        }
        if (strcmp(req->uri, "/api/v1/credentials") == 0) {
            set_credentials(root);
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

static esp_err_t restart_post_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        timeout_restart();

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t state_post_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        httpd_resp_set_type(req, "text/plain");

        if (strcmp(req->uri, "/api/v1/state/authorize") == 0) {
            evse_authorize();
        }
        if (strcmp(req->uri, "/api/v1/state/enable") == 0) {
            evse_set_enabled(true);
        }
        if (strcmp(req->uri, "/api/v1/state/disable") == 0) {
            evse_set_enabled(false);
        }
        if (strcmp(req->uri, "/api/v1/state/available") == 0) {
            evse_set_available(true);
        }
        if (strcmp(req->uri, "/api/v1/state/unavailable") == 0) {
            evse_set_available(false);
        }

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t firmware_update_post_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
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

static esp_err_t firmware_upload_post_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
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

static esp_err_t script_reload_post_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        script_reload();

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t partition_get_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        char* partition = req->uri + strlen("/api/v1/partition/");

        size_t total = 0, used = 0;
        esp_err_t ret = esp_spiffs_info(partition, &total, &used);
        if (ret != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Failed to get SPIFFS partition information");
            return ESP_FAIL;
        }

        char path[ESP_VFS_PATH_MAX];
        sprintf(path, "/%s/", partition);

        DIR* dd = opendir(path);
        if (dd == NULL) {
            ESP_LOGE(TAG, "Failed to open directory %s", path);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Failed to open directory");
            return ESP_FAIL;
        }

        cJSON* root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "total", total);
        cJSON_AddNumberToObject(root, "used", used);

        char entrypath[FILE_PATH_MAX];
        struct dirent* entry;
        struct stat entry_stat;
        const size_t path_len = strlen(path);

        strlcpy(entrypath, path, sizeof(entrypath));

        cJSON* files = cJSON_CreateArray();
        while ((entry = readdir(dd)) != NULL) {
            cJSON* item = cJSON_CreateObject();

            cJSON_AddStringToObject(item, "name", entry->d_name);

            strlcpy(entrypath + path_len, entry->d_name, sizeof(entrypath) - path_len);

            if (stat(entrypath, &entry_stat) == -1) {
                ESP_LOGE(TAG, "Failed to stat %s ", entry->d_name);
            } else {
                cJSON_AddNumberToObject(item, "size", entry_stat.st_size);
            }

            cJSON_AddItemToArray(files, item);
        }

        cJSON_AddItemToObject(root, "files", files);

        const char* json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);

        free((void*)json);

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t fs_file_get_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        char* path = req->uri + strlen("/api/v1/fs");
        char* file = strrchr(path, '/') + 1;

        FILE* fd = fopen(path, "r");
        if (fd == NULL) {
            ESP_LOGE(TAG, "Failed to open file %s", path);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Failed to open file");
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "application/octet-stream");
        char content_disp[64];
        sprintf(content_disp, "attachment; filename=\"%s\"", file);
        httpd_resp_set_hdr(req, "Content-Disposition", content_disp);

        char buf[SCRATCH_BUFSIZE];
        size_t len;
        do {
            len = fread(buf, sizeof(char), SCRATCH_BUFSIZE, fd);
            if (len > 0) {
                if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                    fclose(fd);
                    ESP_LOGE(TAG, "File sending failed");
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

static esp_err_t fs_file_post_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        char* path = req->uri + strlen("/api/v1/fs");

        FILE* fd = fopen(path, "w");
        if (fd == NULL) {
            ESP_LOGE(TAG, "Failed to open file %s", path);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
            return ESP_FAIL;
        }

        int received = 0;
        int remaining = req->content_len;
        char buf[SCRATCH_BUFSIZE];

        while (remaining > 0) {
            if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
                fclose(fd);

                ESP_LOGE(TAG, "File receive failed");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
                return ESP_FAIL;
            }

            if (received != fwrite(buf, sizeof(char), received, fd)) {
                fclose(fd);
                unlink(path);

                ESP_LOGE(TAG, "File write failed");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
                return ESP_FAIL;
            }

            remaining -= received;
        }

        fclose(fd);

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t fs_file_delete_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
        char* path = req->uri + strlen("/api/v1/fs");

        unlink(path);

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, "OK");

        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t log_get_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
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

static esp_err_t script_output_get_handler(httpd_req_t* req)
{
    if (authorize_req(req)) {
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
    config.max_uri_handlers = 14;
    // config.max_open_sockets = 3;
    // config.lru_purge_enable = true;
    // config.open_fn = sess_on_open;
    // config.close_fn = sess_on_close;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    //ESP_LOGI(TAG, "Credentials user / password: %s / %s", user, password);

    httpd_uri_t partition_get_uri = {
        .uri = "/api/v1/partition/*",
        .method = HTTP_GET,
        .handler = partition_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &partition_get_uri));

    httpd_uri_t fs_file_get_uri = {
        .uri = "/api/v1/fs/*",
        .method = HTTP_GET,
        .handler = fs_file_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fs_file_get_uri));

    httpd_uri_t fs_file_post_uri = {
        .uri = "/api/v1/fs/*",
        .method = HTTP_POST,
        .handler = fs_file_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fs_file_post_uri));

    httpd_uri_t fs_file_delete_uri = {
        .uri = "/api/v1/fs/*",
        .method = HTTP_DELETE,
        .handler = fs_file_delete_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fs_file_delete_uri));

    httpd_uri_t log_get_uri = {
        .uri = "/api/v1/log",
        .method = HTTP_GET,
        .handler = log_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &log_get_uri));

    httpd_uri_t script_output_get_uri = {
        .uri = "/api/v1/script/output",
        .method = HTTP_GET,
        .handler = script_output_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &script_output_get_uri));

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

    httpd_uri_t script_reload_post_uri = {
        .uri = "/api/v1/script/reload",
        .method = HTTP_POST,
        .handler = script_reload_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &script_reload_post_uri));

    httpd_uri_t json_post_uri = {
       .uri = "/api/v1/*",
       .method = HTTP_POST,
       .handler = json_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &json_post_uri));

    httpd_uri_t web_get_uri = {
        .uri = "*",
        .method = HTTP_GET,
        .handler = web_get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &web_get_uri));
}
