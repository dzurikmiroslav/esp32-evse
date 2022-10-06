#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "tcp_logger.h"

#define TCP_PORT                3000
#define TCP_MAX_CONN            2
#define TCP_LISTEN_BACKLOG      5

#define SHUTDOWN_TIMEOUT        1000

#define KEEPALIVE_IDLE          5
#define KEEPALIVE_INTERVAL      5
#define KEEPALIVE_COUNT         3

#define NVS_NAMESPACE           "tcp_logger"
#define NVS_ENABLED             "enabled"

static const char* TAG = "tcp_logger";

static nvs_handle nvs;

int socks[TCP_MAX_CONN];

static int listen_sock = -1;

static TaskHandle_t tcp_server_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

static int port_bind(void)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return listen_sock;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TCP_PORT);
    int err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        listen_sock = -1;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", TCP_PORT);

    err = listen(listen_sock, TCP_LISTEN_BACKLOG);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        listen_sock = -1;
    }

    return listen_sock;
}

static int port_accept_conn(int listen_sock)
{
    char addr_str[128];
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;

    int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to accept connection: errno=%d", errno);
        close(sock);
    } else {
        inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "Socket (#%d) accepted ip address: %s", sock, addr_str);
    }

    // Set tcp keepalive option
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

    return sock;
}

static void tcp_server_task_func(void* params)
{
    struct pollfd fds[TCP_MAX_CONN + 1];

    for (int i = 0; i < TCP_MAX_CONN; i++) {
        socks[i] = -1;
        fds[i + 1].events = POLLIN | POLLHUP;
    }

    do {
        listen_sock = port_bind();
    } while (listen_sock < 0 && !shutdown_sem);

    fds[0].fd = listen_sock;
    fds[0].events = POLLIN | POLLHUP;

    while (listen_sock != -1) {
        int conn_cout = 0;

        for (int i = 0; i < TCP_MAX_CONN; i++) {
            if (socks[i] > 0) {
                conn_cout++;
                fds[i + 1].fd = socks[i];
            }
        }

        if (poll(fds, conn_cout + 1, -1) != -1) {
            if (fds[0].revents & POLLIN) {
                int sock = port_accept_conn(listen_sock);
                if (conn_cout >= TCP_MAX_CONN) {
                    ESP_LOGE(TAG, "Maximum connection count %d reached", TCP_MAX_CONN);
                    close(sock);
                } else {
                    socks[conn_cout++] = sock;
                }
            }

            for (int i = 0; i < TCP_MAX_CONN; i++) {
                if (socks[i] > 0) {
                    if (fds[i + 1].revents & POLLIN) {
                        if (recv(socks[i], NULL, 100, MSG_DONTWAIT) < 0) {
                            if (errno == ENOTCONN) {
                                ESP_LOGI(TAG, "Socket (#%d), closing", socks[i]);
                                shutdown(socks[i], SHUT_RDWR);
                                close(socks[i]);
                                socks[i] = -1;
                            } else {
                                ESP_LOGE(TAG, "Socket (#%d), error during receive: errno %d", socks[i], errno);
                            }
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < TCP_MAX_CONN; i++) {
        if (socks[i] > 0) {
            ESP_LOGI(TAG, "Socket (#%d), closing", socks[i]);
            shutdown(socks[i], SHUT_RDWR);
            close(socks[i]);
        }
    }

    if (shutdown_sem) {
        xSemaphoreGive(shutdown_sem);
    }
    vTaskDelete(NULL);
}

static void tcp_server_start(void)
{
    if (!tcp_server_task) {
        ESP_LOGI(TAG, "Starting server");
        xTaskCreate(tcp_server_task_func, "logger_tcp_server", 4096, NULL, 5, &tcp_server_task);
    }
}

static void tcp_server_stop(void)
{
    if (tcp_server_task) {
        ESP_LOGI(TAG, "Stopping server");
        shutdown_sem = xSemaphoreCreateBinary();

        close(listen_sock);
        listen_sock = -1;

        if (!xSemaphoreTake(shutdown_sem, pdMS_TO_TICKS(SHUTDOWN_TIMEOUT))) {
            ESP_LOGE(TAG, "Server task stop timeout, will be force stoped");
            vTaskDelete(tcp_server_task);
        }

        vSemaphoreDelete(shutdown_sem);
        shutdown_sem = NULL;
        tcp_server_task = NULL;
    }
}

void tcp_logger_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    esp_register_shutdown_handler(&tcp_server_stop);

    if (tcp_logger_is_enabled()) {
        tcp_server_start();
    }
}

void tcp_logger_set_enabled(bool enabled)
{
    nvs_set_u8(nvs, NVS_ENABLED, enabled);

    nvs_commit(nvs);

    if (enabled) {
        tcp_server_start();
    } else {
        tcp_server_stop();
    }
}

bool tcp_logger_is_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}

void tcp_logger_log_process(const char* str, int len)
{
    if (tcp_server_task) {
        for (int i = 0; i < TCP_MAX_CONN; i++) {
            if (socks[i] != -1) {
                send(socks[i], str, len, MSG_DONTWAIT);
            }
        }
    }
}