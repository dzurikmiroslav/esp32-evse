#include "http_dav.h"

#include <dirent.h>
#include <errno.h>
#include <esp_littlefs.h>
#include <esp_log.h>
#include <esp_vfs.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http.h"
#include "script.h"

#define DAV_BASE_PATH     "/dav"
#define DAV_BASE_PATH_LEN 4
#define FILE_PATH_MAX     (ESP_VFS_PATH_MAX + CONFIG_LITTLEFS_OBJ_NAME_LEN)
#define SCRATCH_BUFSIZE   1024

static const char* TAG = "http_dav";

static int rm_rf(const char* path)
{
    struct stat sb;
    if (stat(path, &sb) < 0) return -errno;

    int ret = 0;
    if ((sb.st_mode & S_IFMT) == S_IFDIR) {
        DIR* dir = opendir(path);
        if (dir) {
            struct dirent* de;
            while ((de = readdir(dir))) {
                if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                char* rpath;
                if (asprintf(&rpath, "%s/%s", path, de->d_name) > 0) {
                    rm_rf(rpath);
                    free(rpath);
                }
            }
            closedir(dir);
        }
        ret = rmdir(path);
    } else {
        ret = unlink(path);
    }

    return ret;
}

typedef enum {
    RESP_HDR_ROOT,
    RESP_HDR_DIR,
    RESP_HDR_FILE,
    RESP_HDR_NOT_EXIST
} resp_hdr_t;

static void set_resp_hdr(httpd_req_t* req, resp_hdr_t hdr)
{
    httpd_resp_set_hdr(req, "DAV", "1");
    switch (hdr) {
    case RESP_HDR_ROOT:
        httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND");
        break;
    case RESP_HDR_DIR:
        httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND,DELETE,MOVE");
        break;
    case RESP_HDR_FILE:
        httpd_resp_set_hdr(req, "Allow", "OPTIONS,PROPFIND,DELETE,POST,PUT,GET,HEAD,COPY,MOVE");
        break;
    case RESP_HDR_NOT_EXIST:
        httpd_resp_set_hdr(req, "Allow", "OPTIONS,PUT,MKCOL");
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

static void propfind_response_start(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/xml; charset=\"utf-8\"");
    httpd_resp_set_status(req, "207 Multi-Status");

    httpd_resp_send_chunk(req, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<multistatus xmlns=\"DAV:\">\n", HTTPD_RESP_USE_STRLEN);
}

static void propfind_response_end(httpd_req_t* req)
{
    httpd_resp_send_chunk(req, "</multistatus>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NULL, 0);
}

static void propfind_response_directory(httpd_req_t* req, const char* path)
{
    httpd_resp_send_chunk(req, "<response>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<href>" DAV_BASE_PATH, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, path, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</href>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<prop>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<resourcetype><collection/></resourcetype>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</prop>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<status>HTTP/1.1 200 OK</status>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</response>\n", HTTPD_RESP_USE_STRLEN);
}

static void propfind_response_directory_with_quota(httpd_req_t* req, const char* path)
{
    size_t total = 0, used = 0;
    char str[16];
    esp_littlefs_info("storage", &total, &used);

    httpd_resp_send_chunk(req, "<response>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<href>" DAV_BASE_PATH, HTTPD_RESP_USE_STRLEN);
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
    sprintf(str, "%" PRId32, statbuf.st_size);

    httpd_resp_send_chunk(req, "<response>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<href>" DAV_BASE_PATH, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, path, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</href>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<prop>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<resourcetype/>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<getcontentlength>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, str, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</getcontentlength>\n", HTTPD_RESP_USE_STRLEN);
    // httpd_resp_send_chunk(req, "<getcontenttype>text/plain</getcontenttype>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</prop>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<status>HTTP/1.1 200 OK</status>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</propstat>\n", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "</response>\n", HTTPD_RESP_USE_STRLEN);
}

static void send_redirect_append_slash(httpd_req_t* req)
{
    char location[DAV_BASE_PATH_LEN + FILE_PATH_MAX];
    strcpy(location, req->uri);
    strcat(location, "/");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_set_status(req, "301 Moved Permanently");

    httpd_resp_send(req, "", 0);
}

static esp_err_t options_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    ESP_LOGD(TAG, "Options: %s", path);

    struct stat statbuf;
    if (stat(path, &statbuf) == 0) {
        if (S_ISDIR(statbuf.st_mode)) {
            set_resp_hdr(req, RESP_HDR_DIR);
            if (path[strlen(path) - 1] != '/') {
                send_redirect_append_slash(req);
                return ESP_OK;
            }
        } else {
            set_resp_hdr(req, RESP_HDR_FILE);
        }
    } else {
        set_resp_hdr(req, RESP_HDR_NOT_EXIST);
    }

    httpd_resp_send(req, "", 0);

    return ESP_OK;
}

static esp_err_t propfind_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    ESP_LOGD(TAG, "Propfind: %s", path);

    struct stat statbuf;
    if (stat(path, &statbuf) == 0) {
        if (S_ISREG(statbuf.st_mode)) {
            set_resp_hdr(req, RESP_HDR_FILE);
            propfind_response_start(req);
            propfind_response_file(req, path);
            propfind_response_end(req);
        }
        if (S_ISDIR(statbuf.st_mode)) {
            set_resp_hdr(req, RESP_HDR_DIR);
            if (path[strlen(path) - 1] == '/') {
                int depth = get_hdr_depth(req);
                propfind_response_start(req);
                propfind_response_directory_with_quota(req, path);
                if (depth > 0) {
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
                        if (entry->d_type == DT_DIR) {
                            propfind_response_directory(req, entrypath);
                        }
                        if (entry->d_type == DT_REG) {
                            propfind_response_file(req, entrypath);
                        }
                    }
                    closedir(dd);
                }
                propfind_response_end(req);
            } else {
                send_redirect_append_slash(req);
                return ESP_OK;
            }
        }
    } else {
        if (strcmp(path, "") == 0) {
            set_resp_hdr(req, RESP_HDR_ROOT);
            send_redirect_append_slash(req);
            return ESP_OK;
        } else if (strcmp(path, "/") == 0) {
            set_resp_hdr(req, RESP_HDR_ROOT);
            propfind_response_start(req);
            propfind_response_directory_with_quota(req, "/");
            propfind_response_directory(req, "/storage/");
            propfind_response_end(req);
        } else {
            set_resp_hdr(req, RESP_HDR_DIR);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, NULL);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    ESP_LOGD(TAG, "Get: %s", path);

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

    ESP_LOGD(TAG, "Put: %s", path);

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
            return ESP_FAIL;
        }

        if (received != fwrite(buf, sizeof(char), received, fd)) {
            fclose(fd);
            unlink(path);

            ESP_LOGE(TAG, "File write failed");
            httpd_resp_send_custom_err(req, "531 Failed To Write File", "Failed to write file");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    fclose(fd);

    script_file_changed(path);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t mkcol_handler(httpd_req_t* req)
{
    char path[FILE_PATH_MAX];
    strcpy(path, req->uri + DAV_BASE_PATH_LEN);

    ESP_LOGD(TAG, "Mkcol: %s", path);

    int len = strlen(path);
    if (path[len - 1] == '/') path[len - 1] = '\0';

    int ret = mkdir(path, 0755);
    if (ret == 0) {
        httpd_resp_set_status(req, "201 Created");
        httpd_resp_send_chunk(req, NULL, 0);
    } else {
        switch (errno) {
        case EEXIST:
            httpd_resp_send_custom_err(req, "409 Conflict", "Conflict");
            break;
        case ENOENT:
            httpd_resp_send_custom_err(req, "507 Insufficient Storage", "Insufficient Storage");
            break;
        default:
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, NULL);
            break;
        }
        httpd_resp_send_chunk(req, NULL, 0);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t delete_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    ESP_LOGD(TAG, "Delete: %s", path);

    rm_rf(path);

    script_file_changed(path);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t move_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    char dest[FILE_PATH_MAX];
    get_hdr_dest(req, dest);

    ESP_LOGD(TAG, "Move: %s -> %s", path, dest);

    rename(path, dest);

    script_file_changed(path);
    script_file_changed(dest);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t copy_handler(httpd_req_t* req)
{
    const char* path = req->uri + DAV_BASE_PATH_LEN;

    char dest[FILE_PATH_MAX];
    get_hdr_dest(req, dest);

    ESP_LOGD(TAG, "Copy: %s -> %s", path, dest);

    // TODO copy whole directory

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

    script_file_changed(dest);

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

size_t http_dav_handlers_count(void)
{
    return 9;
}

void http_dav_add_handlers(httpd_handle_t server)
{
    httpd_uri_t options_uri = {
        .uri = DAV_BASE_PATH "/?*",
        .method = HTTP_OPTIONS,
        .handler = options_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &options_uri));

    httpd_uri_t propfind_uri = {
        .uri = DAV_BASE_PATH "/?*",
        .method = HTTP_PROPFIND,
        .handler = propfind_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &propfind_uri));

    httpd_uri_t head_uri = {
        .uri = DAV_BASE_PATH "/*",
        .method = HTTP_HEAD,
        .handler = get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &head_uri));

    httpd_uri_t get_uri = {
        .uri = DAV_BASE_PATH "/*",
        .method = HTTP_GET,
        .handler = get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));

    httpd_uri_t put_uri = {
        .uri = DAV_BASE_PATH "/*",
        .method = HTTP_PUT,
        .handler = put_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &put_uri));

    httpd_uri_t mkcol_uri = {
        .uri = DAV_BASE_PATH "/*",
        .method = HTTP_MKCOL,
        .handler = mkcol_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &mkcol_uri));

    httpd_uri_t delete_uri = {
        .uri = DAV_BASE_PATH "/*",
        .method = HTTP_DELETE,
        .handler = delete_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &delete_uri));

    httpd_uri_t move_uri = {
        .uri = DAV_BASE_PATH "/*",
        .method = HTTP_MOVE,
        .handler = move_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &move_uri));

    httpd_uri_t copy_uri = {
        .uri = DAV_BASE_PATH "/*",
        .method = HTTP_COPY,
        .handler = copy_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &copy_uri));
}