#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "http_web.h"
#include "http.h"

#define DAV_BASE_PATH           "/dav"
#define DAV_BASE_PATH_LEN       4
#define FILE_PATH_MAX           (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define SCRATCH_BUFSIZE         1024

static const char* TAG = "http_dav";

typedef enum
{
    RESP_HDR_DIR,
    RESP_HDR_FILE_NOT_EXIST,
    RESP_HDR_FILE_EXIST
} resp_hdr_t;

static void set_resp_hdr(httpd_req_t* req, resp_hdr_t hdr)
{
    httpd_resp_set_hdr(req, "DAV", "1");
    switch (hdr)
    {
    case RESP_HDR_DIR:
        httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND");
        break;
    case RESP_HDR_FILE_NOT_EXIST:
        httpd_resp_set_hdr(req, "Allow", "OPTIONS,PUT");
        break;
    case RESP_HDR_FILE_EXIST:
        httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND,DELETE,POST,PUT,GET,HEAD,COPY,MOVE");
        break;
    }
}

static int get_hdr_depth(httpd_req_t* req)
{
    char depth[16];
    httpd_req_get_hdr_value_str(req, "Depth", depth, sizeof(depth));
    if (strcmp(depth, "") == 0 || strcmp(depth, "infinity") == 0) {
        return -1;
    }

    return atoi(depth);
}

static void get_hdr_dest(httpd_req_t* req, char* out)
{
    char dest[64];
    httpd_req_get_hdr_value_str(req, "Destination", dest, sizeof(dest));

    char host[32];
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));

    char* sub_dest = strstr(dest, host);
    if (sub_dest != NULL) {
        sub_dest += strlen(host);
    } else {
        sub_dest = dest;
    }

    strcpy(out, sub_dest + DAV_BASE_PATH_LEN);
}

static void propfind_response_directory(httpd_req_t* req, const char* path)
{
    size_t total = 0, used = 0;
    char str[16];
    if (strcmp(path, "/cfg/") == 0) {
        esp_spiffs_info("cfg", &total, &used);
    }
    if (strcmp(path, "/data/") == 0) {
        esp_spiffs_info("data", &total, &used);
    }

    httpd_resp_send_chunk(req, "<response>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<href>"DAV_BASE_PATH, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, path, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</href>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<prop>\n", HTTPD_RESP_USE_STRLEN);
    sprintf(str, "%zu", used);
    httpd_resp_send_chunk(req, "<quota-used-bytes>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, str, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</quota-used-bytes>\n", HTTPD_RESP_USE_STRLEN);
    sprintf(str, "%zu", total - used);
    httpd_resp_send_chunk(req, "<quota-available-bytes>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, str, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</quota-available-bytes>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<resourcetype><collection/></resourcetype>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</prop>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<status>HTTP/1.1 200 OK</status>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</response>\n", HTTPD_RESP_USE_STRLEN);
}

static void propfind_response_file(httpd_req_t* req, const char* path)
{
    struct stat statbuf;
    char str[16];
    stat(path, &statbuf);
    sprintf(str, "%"PRId32, statbuf.st_size);

    httpd_resp_send_chunk(req, "<response>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<href>"DAV_BASE_PATH, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, path, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</href>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<prop>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<resourcetype/>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<getcontentlength>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, str, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</getcontentlength>\n", HTTPD_RESP_USE_STRLEN);
    //    httpd_resp_send_chunk(req, "<getcontenttype>text/plain</getcontenttype>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</prop>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<status>HTTP/1.1 200 OK</status>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</response>\n", HTTPD_RESP_USE_STRLEN);
}


static esp_err_t options_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    if (strcmp(path, "/") == 0 || strcmp(path, "/data/") == 0 || strcmp(path, "/cfg/") == 0) {
        set_resp_hdr(req, RESP_HDR_DIR);
        //directories
    } else if (strcmp(path, "/data") == 0 || strcmp(path, "/cfg") == 0) {
        //redirects
        char location[8];
        strcpy(location, req->uri);
        strcat(location, "/");
        httpd_resp_set_hdr(req, "Location", location);
        httpd_resp_set_status(req, "301 Moved Permanently");
        httpd_resp_send(req, "", 0);
        return ESP_OK;
    } else {
        //files
        struct stat statbuf;
        if (stat(path, &statbuf) == 0 && S_ISREG(statbuf.st_mode)) {
            set_resp_hdr(req, RESP_HDR_FILE_EXIST);
        } else {
            set_resp_hdr(req, RESP_HDR_FILE_NOT_EXIST);
        }
    }

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t propfind_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    if (strcmp(path, "/") == 0 || strcmp(path, "/data/") == 0 || strcmp(path, "/cfg/") == 0) {
        //directories
        set_resp_hdr(req, RESP_HDR_DIR);
    } else if (strcmp(path, "") == 0 || strcmp(path, "/data") == 0 || strcmp(path, "/cfg") == 0) {
        //redirects
        char location[8];
        strcpy(location, req->uri);
        strcat(location, "/");
        httpd_resp_set_hdr(req, "Location", location);
        httpd_resp_set_status(req, "301 Moved Permanently");
        httpd_resp_send(req, "", 0);
        return ESP_OK;
    } else {
        //files
        struct stat statbuf;
        if (stat(path, &statbuf) != 0 || !S_ISREG(statbuf.st_mode)) {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
            return ESP_FAIL;
        }
        set_resp_hdr(req, RESP_HDR_FILE_EXIST);
    }

    int depth = get_hdr_depth(req);

    httpd_resp_set_type(req, "text/xml; charset=\"utf-8\"");
    httpd_resp_set_status(req, "207 Multi-Status");

    httpd_resp_send_chunk(req, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<multistatus xmlns=\"DAV:\">\n", HTTPD_RESP_USE_STRLEN);

    if (strcmp(path, "/") == 0) {
        propfind_response_directory(req, "/");
        if (depth != 0) {
            propfind_response_directory(req, "/cfg/");
            propfind_response_directory(req, "/data/");
        }
    } else if (strcmp(path, "/data/") == 0 || strcmp(path, "/cfg/") == 0) {
        propfind_response_directory(req, path);
        if (depth != 0) {
            DIR* dd = opendir(path);
            if (dd == NULL) {
                ESP_LOGE(TAG, "Failed to open directory %s", path);
                httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
                return ESP_FAIL;
            }

            struct dirent* entry;
            char entrypath[FILE_PATH_MAX];
            while ((entry = readdir(dd)) != NULL) {
                strcpy(entrypath, path);
                strcat(entrypath, entry->d_name);
                propfind_response_file(req, entrypath);
            }
            closedir(dd);
        }
    } else {
        propfind_response_file(req, path);
    }

    httpd_resp_send_chunk(req, "</multistatus>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    FILE* fd = fopen(path, "r");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s", path);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/octet-stream");

    if (req->method == HTTP_GET) {
        char buf[SCRATCH_BUFSIZE];
        size_t len;
        do {
            len = fread(buf, sizeof(char), SCRATCH_BUFSIZE, fd);
            if (len > 0) {
                if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                    fclose(fd);
                    ESP_LOGE(TAG, "File sending failed");
                    httpd_resp_sendstr_chunk(req, NULL);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
                    return ESP_FAIL;
                }
            }
        } while (len != 0);
    }

    fclose(fd);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t put_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    FILE* fd = fopen(path, "w");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s", path);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
        return ESP_FAIL;
    }

    int received = 0;
    int remaining = req->content_len;
    char buf[SCRATCH_BUFSIZE];

    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            fclose(fd);

            ESP_LOGE(TAG, "File receive failed");
            httpd_resp_send_custom_err(req, "530 Failed To Receive File", "Failed to receive file");
//            httpd_resp_send_err(req, HTTPD_530_INTERNAL_SERVER_ERROR, "Failed to receive file");
            return ESP_FAIL;
        }

        if (received != fwrite(buf, sizeof(char), received, fd)) {
            fclose(fd);
            unlink(path);

            ESP_LOGE(TAG, "File write failed");
            httpd_resp_send_custom_err(req, "531 Failed To Write File", "Failed to write file");
            //httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    fclose(fd);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t delete_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    unlink(path);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t move_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    char dest[FILE_PATH_MAX];
    get_hdr_dest(req, dest);

    rename(path, dest);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t copy_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    char dest[FILE_PATH_MAX];
    get_hdr_dest(req, dest);

    FILE* src_fd = fopen(path, "r");
    if (src_fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s", path);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
        return ESP_FAIL;
    }

    FILE* dst_fd = fopen(dest, "w");
    if (dst_fd == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s", dest);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
        if (src_fd != NULL) {
            fclose(src_fd);
        }
        return ESP_FAIL;
    }

    char buf[SCRATCH_BUFSIZE];
    size_t len;
    do {
        len = fread(buf, sizeof(char), SCRATCH_BUFSIZE, src_fd);
        if (len > 0) {
            if (len != fwrite(buf, sizeof(char), len, dst_fd)) {
                fclose(src_fd);
                fclose(dst_fd);
                unlink(dest);

                ESP_LOGE(TAG, "File write failed %s", dest);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
                return ESP_FAIL;
            }

        }
    } while (len != 0);

    fclose(src_fd);
    fclose(dst_fd);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

size_t http_dav_handlers_count(void)
{
    return 8;
}

void http_dav_add_handlers(httpd_handle_t server)
{
    httpd_uri_t options_uri = {
        .uri = DAV_BASE_PATH"/?*",
        .method = HTTP_OPTIONS,
        .handler = options_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &options_uri));

    httpd_uri_t propfind_uri = {
        .uri = DAV_BASE_PATH"/?*",
        .method = HTTP_PROPFIND,
        .handler = propfind_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &propfind_uri));

    httpd_uri_t head_uri = {
        .uri = DAV_BASE_PATH"/*",
        .method = HTTP_HEAD,
        .handler = get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &head_uri));

    httpd_uri_t get_uri = {
        .uri = DAV_BASE_PATH"/*",
        .method = HTTP_GET,
        .handler = get_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));

    httpd_uri_t put_uri = {
        .uri = DAV_BASE_PATH"/*",
        .method = HTTP_PUT,
        .handler = put_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &put_uri));

    httpd_uri_t delete_uri = {
        .uri = DAV_BASE_PATH"/*",
        .method = HTTP_DELETE,
        .handler = delete_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &delete_uri));

    httpd_uri_t move_uri = {
        .uri = DAV_BASE_PATH"/*",
        .method = HTTP_MOVE,
        .handler = move_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &move_uri));

    httpd_uri_t copy_uri = {
       .uri = DAV_BASE_PATH"/*",
       .method = HTTP_COPY,
       .handler = copy_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &copy_uri));
}