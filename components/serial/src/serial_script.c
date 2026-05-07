#include "serial_script.h"

#include <esp_log.h>
#include <sys/param.h>
#include <unistd.h>

#include "serial_mode.h"

static const char* TAG = "serial_script";

bool serial_script_is_available(void)
{
    int fd = serial_fd_find(SERIAL_MODE_SCRIPT_NAME);

    return fd != -1;
}

esp_err_t serial_script_write(const char* buf, size_t len)
{
    int fd = serial_fd_find(SERIAL_MODE_SCRIPT_NAME);

    if (fd == -1) {
        ESP_LOGW(TAG, "No script serial available");
        return ESP_ERR_NOT_FOUND;
    }

    write(fd, buf, len);

    return ESP_OK;
}

esp_err_t serial_script_flush(void)
{
    int fd = serial_fd_find(SERIAL_MODE_SCRIPT_NAME);

    if (fd == -1) {
        ESP_LOGW(TAG, "No script serial available");
        return ESP_ERR_NOT_FOUND;
    }

    fsync(fd);

    return ESP_OK;
}

esp_err_t serial_script_read(char* buf, size_t* len)
{
    int fd = serial_fd_find(SERIAL_MODE_SCRIPT_NAME);

    if (fd == -1) {
        ESP_LOGW(TAG, "No script serial available");
        return ESP_ERR_NOT_FOUND;
    }

    int readed = read(fd, buf, *len);
    *len = MAX(0, readed);

    return ESP_OK;
}