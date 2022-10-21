#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
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
#define TCP_MAX_CONN            3
#define TCP_LISTEN_BACKLOG      5
#define TCP_BUF_SIZE            32

#define SHUTDOWN_TIMEOUT        1000

#define KEEPALIVE_IDLE          5
#define KEEPALIVE_INTERVAL      5
#define KEEPALIVE_COUNT         3

#define NVS_NAMESPACE           "tcp_logger"
#define NVS_ENABLED             "enabled"

#define LOG_LVL_CONN            ESP_LOG_DEBUG

static const char* TAG = "tcp_logger";

static nvs_handle nvs;

static int socks[TCP_MAX_CONN];

static int listen_sock = -1;

static TaskHandle_t tcp_server_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

static EventGroupHandle_t sock_lock = NULL;

static void close_conn(int* sock)
{
    if (shutdown(*sock, SHUT_RDWR) == -1) {
        ESP_LOGE(TAG, "Socket (#%d), shutdown failed: errno %d", *sock, errno);
    } else {
        ESP_LOG_LEVEL(LOG_LVL_CONN, TAG, "Socket (#%d), closed", *sock);
    }
    close(*sock);
}

static int accept_conn(int listen_sock)
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
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        close(sock);
    } else {
        inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOG_LEVEL(LOG_LVL_CONN, TAG, "Socket (#%d), accepted ip address: %s", sock, addr_str);
    }

    // Set tcp keepalive option
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

    return sock;
}

static int port_bind(void)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return listen_sock;
    }

    int opt = 1;
    int err = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during setsockopt: errno %d", errno);
        close(listen_sock);
        listen_sock = -1;
    }

    struct sockaddr_in dest_addr = {
          .sin_family = PF_INET,
          .sin_addr = {
              .s_addr = htonl(INADDR_ANY)
          },
          .sin_port = htons(TCP_PORT)
    };
    err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
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

static void tcp_server_task_func(void* params)
{
    for (int i = 0; i < TCP_MAX_CONN; i++) {
        socks[i] = -1;
    }

    do {
        listen_sock = port_bind();
    } while (listen_sock < 0 && !shutdown_sem);

    while (listen_sock != -1) {
        fd_set read_set;
        int max_fd = listen_sock;
        FD_ZERO(&read_set);
        FD_SET(listen_sock, &read_set);

        int conn_cout = 0;

        for (int i = 0; i < TCP_MAX_CONN; i++) {
            if (socks[i] > 0) {
                conn_cout++;
                FD_SET(socks[i], &read_set);
                max_fd = MAX(max_fd, socks[i]);
            }
        }

        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(listen_sock, &read_set)) {
                int sock = accept_conn(listen_sock);
                if (conn_cout >= TCP_MAX_CONN) {
                    ESP_LOGE(TAG, "Maximum connection count %d reached", TCP_MAX_CONN);
                    close_conn(&sock);
                } else {
                    socks[conn_cout++] = sock;
                }
            }

            for (int i = 0; i < TCP_MAX_CONN; i++) {
                if (socks[i] > 0) {
                    if (FD_ISSET(socks[i], &read_set)) {
                        char buf[TCP_BUF_SIZE];
                        if (recv(socks[i], buf, TCP_BUF_SIZE, MSG_DONTWAIT) < 0) {
                            if (errno != ENOTCONN) {
                                ESP_LOGE(TAG, "Socket (#%d), error during receive: errno %d", socks[i], errno);
                            }
                            close_conn(&socks[i]);
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < TCP_MAX_CONN; i++) {
        if (socks[i] > 0) {
            close_conn(&socks[i]);
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
    sock_lock = xEventGroupCreate();
    xEventGroupSetBits(sock_lock, 0xFF);

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

void tcp_logger_print(const char* str, int len)
{
    if (tcp_server_task) {
        for (int i = 0; i < TCP_MAX_CONN; i++) {
            EventBits_t bit = 1 << i;
            if (xEventGroupWaitBits(sock_lock, bit, pdTRUE, pdFALSE, 0) & bit) { // prevent recursion
                if (socks[i] != -1) {
                    send(socks[i], str, len, MSG_DONTWAIT);                   
                }
                xEventGroupSetBits(sock_lock, bit);
            }
        }
    }
}