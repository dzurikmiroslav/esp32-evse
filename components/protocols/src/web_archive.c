#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "web_archive.h"

#define CPIO_MAGIC          "070707"
#define CPIO_MAGIC_LENGTH   6
#define CPIO_TRAILER        "TRAILER!!!"

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
}cpio_header_t;

static const char* TAG = "web_archive";

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


size_t web_archive_find(const char* name, char** content)
{
    cpio_header_t* hdr = (cpio_header_t*)web_cpio_start;

    while (cpio_next(&hdr)) {
        if (strcmp(name, cpio_file_name(hdr)) == 0) {
            *content = (char*)cpio_file_data(hdr);
            return  cpio_file_size(hdr);
        }
    }

    return 0;
}