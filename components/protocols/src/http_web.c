#include "http_web.h"

#include <esp_log.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "http.h"

#define CPIO_MAGIC        "070707"
#define CPIO_MAGIC_LENGTH 6
#define CPIO_TRAILER      "TRAILER!!!"

typedef struct {
    char magic[6];
    char dev[6];
    char ino[6];
    char mode[6];
    char uid[6];
    char gid[6];
    char nlink[6];
    char rdev[6];
    char mtime[11];
    char name_size[6];
    char file_size[11];
} cpio_header_t;

static const char* TAG = "http_web";

extern const char web_cpio_start[] asm("_binary_web_cpio_start");
extern const char web_cpio_end[] asm("_binary_web_cpio_end");

static size_t cpio_file_size(cpio_header_t* hdr)
{
    uint32_t size;
    sscanf(hdr->file_size, "%11lo", &size);
    return size;
}

static size_t cpio_name_size(cpio_header_t* hdr)
{
    uint16_t size;
    sscanf(hdr->name_size, "%6ho", &size);
    return size;
}

static const char* cpio_file_name(cpio_header_t* hdr)
{
    return ((char*)hdr) + sizeof(cpio_header_t);
}

static const char* cpio_file_data(cpio_header_t* hdr)
{
    return ((char*)hdr) + sizeof(cpio_header_t) + cpio_name_size(hdr);
}

static bool cpio_next(cpio_header_t** hdr)
{
    if (strncmp((*hdr)->magic, CPIO_MAGIC, CPIO_MAGIC_LENGTH) != 0) {
        ESP_LOGE(TAG, "Bad archive format");
        return false;
    }

    *hdr = (cpio_header_t*)(((char*)*hdr) + sizeof(cpio_header_t) + cpio_name_size(*hdr) + cpio_file_size(*hdr));

    if (((char*)*hdr) >= web_cpio_end) {
        return false;
    }

    if (strcmp(cpio_file_name(*hdr), CPIO_TRAILER) == 0) {
        return false;
    }

    return true;
}

static size_t web_archive_find(const char* name, char** content)
{
    cpio_header_t* hdr = (cpio_header_t*)web_cpio_start;

    do {
        if (strcmp(name, cpio_file_name(hdr)) == 0) {
            *content = (char*)cpio_file_data(hdr);
            return cpio_file_size(hdr);
        }
    } while (cpio_next(&hdr));

    return 0;
}

static esp_err_t get_handler(httpd_req_t* req)
{
    if (http_authorize_req(req)) {
        char file_name[HTTPD_MAX_URI_LEN];
        strcpy(file_name, req->uri + 1);
        char* file_suffix = strrchr(file_name, '.');
        strcat(file_name, ".gz");

        char* file_data;
        uint32_t file_size = web_archive_find(file_name, &file_data);
        if (file_size == 0) {
            // fallback to index.html
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

size_t http_web_handlers_count(void)
{
    return 1;
}

void http_web_add_handlers(httpd_handle_t server)
{
    httpd_uri_t get_uri = {
        .uri = "*",
        .method = HTTP_GET,
        .handler = get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_uri));
}