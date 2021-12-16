#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "tcp_logger.h"

#define KEEPALIVE_IDLE      5
#define KEEPALIVE_INTERVAL  5
#define KEEPALIVE_COUNT     3
#define DEFAULT_PORT        3000

#define NVS_NAMESPACE       "evse_tcplog"
#define NVS_ENABLED         "enabled"
#define NVS_PORT            "port"

static const char* TAG = "tcp_logger";

static nvs_handle nvs;

static int sock = -1;

static int listen_sock = -1;

static int tcp_logging_vprintf(const char* str, va_list l)
{
    if (sock >= 0) {
        char buf[256];
        int len = vsprintf((char*)buf, str, l);

        int written = send(sock, buf, len, 0);
        if (written < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        }
    }

    return vprintf(str, l);
}

static void do_retransmit(const int sock)
{
    int len;
    char rx_buffer[32];

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGI(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            int to_write = len;
            while (to_write > 0) {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                }
                to_write -= written;
            }
        }
    } while (len > 0);
}

static void tcp_server_task(void* pvParameters)
{
    char addr_str[128];
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    uint16_t port = DEFAULT_PORT;
    nvs_get_u16(nvs, NVS_PORT, &port);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    int err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", port);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    for (;;) {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
        sock = -1;

        ESP_LOGI(TAG, "Socket closed");
    }

CLEAN_UP:
    close(listen_sock);
    listen_sock = -1;
    vTaskDelete(NULL);
}

static void try_start(void)
{
    if (tcp_logger_get_enabled()) {
        xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    }
}

static void tcp_server_stop(void)
{
    if (sock >= 0) {
        close(sock);
        sock = -1;
    }

    if (listen_sock >= 0) {
        close(listen_sock);
        listen_sock = -1;
    }
}

void tcp_logger_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    esp_log_set_vprintf(tcp_logging_vprintf);

    esp_register_shutdown_handler(&tcp_server_stop);

    try_start();
}

void tcp_logger_set_config(bool enabled, uint16_t port)
{
    nvs_set_u8(nvs, NVS_ENABLED, enabled);
    port = MAX(1000, port);
    nvs_set_u16(nvs, NVS_PORT, port);

    nvs_commit(nvs);

    tcp_server_stop();

    try_start();
}

bool tcp_logger_get_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}

uint16_t tcp_logger_get_port(void)
{
    uint16_t value = DEFAULT_PORT;
    nvs_get_u16(nvs, NVS_PORT, &value);
    return value;
}