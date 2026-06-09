#include "syslog.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <nvs.h>
#include <string.h>
#include <time.h>

#include "discovery.h"
#include "logger.h"

#define NVS_NAMESPACE "syslog"
#define NVS_ENABLED   "enabled"
#define NVS_HOST      "host"

#define SYSLOG_PORT        514
#define SYSLOG_FACILITY    1  // RFC 3164 facility "user"
#define SYSLOG_QUEUE_LEN   16
#define SYSLOG_PACKET_SIZE 600

static const char* TAG = "syslog";

static nvs_handle_t nvs;
static SemaphoreHandle_t s_cfg_mutex;

static volatile bool s_enabled;
static char s_host[SYSLOG_HOST_SIZE];

static QueueHandle_t s_queue;
static int s_sock = -1;
static struct sockaddr_in s_dest;
static bool s_dest_valid;
static volatile bool s_cfg_dirty;

static void syslog_line_cb(const char* line, int len)
{
    if (!s_enabled || s_queue == NULL) {
        return;
    }

    char* copy = malloc(len + 1);
    if (copy == NULL) {
        return;
    }
    memcpy(copy, line, len);
    copy[len] = '\0';

    if (xQueueSend(s_queue, &copy, 0) != pdTRUE) {
        free(copy);  // queue full: drop the newest line, never block logging
    }
}

static int severity_from_line(const char* line)
{
    const char* p = line;
    if (*p == '\033') {  // skip leading ANSI color escape: ESC [ ... m
        while (*p != '\0' && *p != 'm') {
            p++;
        }
        if (*p == 'm') {
            p++;
        }
    }
    switch (*p) {
    case 'E':
        return 3;  // err
    case 'W':
        return 4;  // warning
    case 'I':
        return 6;  // info
    case 'D':
    case 'V':
        return 7;  // debug
    default:
        return 6;  // info
    }
}

static void strip_line(const char* src, char* dst, size_t dst_size)
{
    size_t i = 0;
    size_t j = 0;
    while (src[i] != '\0' && j < dst_size - 1) {
        if (src[i] == '\033') {  // drop ANSI color escape sequence: ESC ... 'm'
            while (src[i] != '\0' && src[i] != 'm') {
                i++;
            }
            if (src[i] != '\0') {
                i++;  // also drop the trailing 'm'
            }
        } else if (src[i] == '\n' || src[i] == '\r') {
            i++;
        } else {
            dst[j++] = src[i];
            i++;
        }
    }
    dst[j] = '\0';
}

static int format_packet(char* buf, size_t buf_size, const char* line)
{
    int pri = SYSLOG_FACILITY * 8 + severity_from_line(line);

    char ts[32];
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(ts, sizeof(ts), "%b %e %H:%M:%S", &tm_info);

    char hostname[DISCOVERY_NAME_SIZE];
    discovery_get_hostname(hostname);

    char msg[SYSLOG_PACKET_SIZE];
    strip_line(line, msg, sizeof(msg));

    return snprintf(buf, buf_size, "<%d>%s %s evse: %s", pri, ts, hostname, msg);
}

static void resolve_dest(void)
{
    s_dest_valid = false;

    char host[SYSLOG_HOST_SIZE];
    syslog_get_host(host);
    if (host[0] == '\0') {
        return;
    }

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM };
    struct addrinfo* res = NULL;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || res == NULL) {
        return;
    }

    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    s_dest.sin_family = AF_INET;
    s_dest.sin_port = htons(SYSLOG_PORT);
    s_dest.sin_addr = addr->sin_addr;
    s_dest_valid = true;

    freeaddrinfo(res);
}

static void send_line(const char* line)
{
    if (!s_enabled) {
        return;
    }

    if (s_cfg_dirty) {
        resolve_dest();
        s_cfg_dirty = false;
    }
    if (!s_dest_valid) {
        return;
    }

    if (s_sock < 0) {
        s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
    if (s_sock < 0) {
        return;
    }

    char packet[SYSLOG_PACKET_SIZE];
    int len = format_packet(packet, sizeof(packet), line);
    if (len <= 0) {
        return;
    }
    if (len >= (int)sizeof(packet)) {
        len = sizeof(packet) - 1;
    }

    int sent = sendto(s_sock, packet, len, 0, (struct sockaddr*)&s_dest, sizeof(s_dest));
    if (sent < 0) {
        s_cfg_dirty = true;  // force re-resolve next time
    }
}

static void syslog_task(void* arg)
{
    char* line;
    while (true) {
        if (xQueueReceive(s_queue, &line, portMAX_DELAY) == pdTRUE) {
            send_line(line);
            free(line);
        }
    }
}

void syslog_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    s_cfg_mutex = xSemaphoreCreateMutex();

    uint8_t enabled = false;
    nvs_get_u8(nvs, NVS_ENABLED, &enabled);
    s_enabled = enabled;

    size_t len = SYSLOG_HOST_SIZE;
    s_host[0] = '\0';
    nvs_get_str(nvs, NVS_HOST, s_host, &len);

    s_cfg_dirty = true;

    s_queue = xQueueCreate(SYSLOG_QUEUE_LEN, sizeof(char*));
    xTaskCreate(syslog_task, "syslog", 4096, NULL, 5, NULL);

    logger_register_line_cb(syslog_line_cb);

    ESP_LOGI(TAG, "Syslog init: enabled=%d host='%s'", s_enabled, s_host);
}

bool syslog_is_enabled(void)
{
    return s_enabled;
}

void syslog_get_host(char* value)
{
    xSemaphoreTake(s_cfg_mutex, portMAX_DELAY);
    snprintf(value, SYSLOG_HOST_SIZE, "%s", s_host);
    xSemaphoreGive(s_cfg_mutex);
}

esp_err_t syslog_set_config(bool enabled, const char* host)
{
    if (host != NULL && strnlen(host, SYSLOG_HOST_SIZE) >= SYSLOG_HOST_SIZE) {
        ESP_LOGE(TAG, "Host out of range");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_set_u8(nvs, NVS_ENABLED, enabled);
    if (host != NULL) {
        nvs_set_str(nvs, NVS_HOST, host);
    }
    nvs_commit(nvs);

    xSemaphoreTake(s_cfg_mutex, portMAX_DELAY);
    if (host != NULL) {
        snprintf(s_host, sizeof(s_host), "%s", host);
    }
    s_enabled = enabled;
    s_cfg_dirty = true;
    xSemaphoreGive(s_cfg_mutex);

    ESP_LOGI(TAG, "Syslog config changed: enabled=%d host='%s'", enabled, s_host);
    return ESP_OK;
}
