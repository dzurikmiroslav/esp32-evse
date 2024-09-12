#include "modbus_tcp.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <nvs.h>
#include <stdint.h>
#include <sys/param.h>

#include "modbus.h"

#define TCP_PORT     502
#define TCP_MAX_CONN 3
#define TCP_BACKLOG  5
#define TCP_BUF_SIZE (MODBUS_PACKET_SIZE + 7)

#define RESPONSE_TIMEOUT   999
#define DISCONNECT_TIMEOUT 20000
#define SHUTDOWN_TIMEOUT   1000

#define MODBUS_TCP_TID  0
#define MODBUS_TCP_PID  2
#define MODBUS_TCP_LEN  4
#define MODBUS_TCP_DATA 6

#define NVS_NAMESPACE "modbus_tcp"
#define NVS_ENABLED   "enabled"

#define LOG_LVL_DATA ESP_LOG_VERBOSE
#define LOG_LVL_CONN ESP_LOG_VERBOSE

static const char* TAG = "modbus_tcp";

static nvs_handle nvs;

static int listen_sock = -1;

static TaskHandle_t tcp_server_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

static int rx_poll(const int sock, uint8_t* buf)
{
    int ret;
    int buf_pos = 0;

    while (1) {
        ret = recv(sock, &buf[buf_pos], sizeof(uint8_t) * TCP_BUF_SIZE, MSG_DONTWAIT);

        if (ret == 0) {
            ret = ERR_CLSD;
            break;
        } else if (ret < 0) {
            if (ret == -1) {
                // MSG_DONTWAIT return -1 when no data left
                ret = buf_pos;
            }
            break;
        } else {
            buf_pos += ret;
            if (buf_pos >= TCP_BUF_SIZE) {
                ESP_LOGW(TAG, "Socket(#%d), incorrect request buffer size %d", sock, buf_pos);
                ret = ERR_BUF;
                break;
            }
        }
    }

    return ret;
}

static void close_conn(int* sock)
{
    if (shutdown(*sock, SHUT_RDWR) == -1) {
        ESP_LOGE(TAG, "Socket (#%d), shutdown failed: errno %d", *sock, errno);
    } else {
        ESP_LOG_LEVEL(LOG_LVL_CONN, TAG, "Socket (#%d), closed", *sock);
    }
    close(*sock);
    *sock = -1;
}

static int accept_conn(int listen_sock)
{
    char addr_str[128];
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);

    int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
        close(sock);
    } else {
        inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOG_LEVEL(LOG_LVL_CONN, TAG, "Socket (#%d), accepted ip address: %s", sock, addr_str);
    }

    return sock;
}

static int port_bind(void)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // TODO IPPROTO_UDP IPPROTO_TCP
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
            .s_addr = htonl(INADDR_ANY),
        },
        .sin_port = htons(TCP_PORT),
    };
    err = bind(listen_sock, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        listen_sock = -1;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", TCP_PORT);

    err = listen(listen_sock, TCP_BACKLOG);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        listen_sock = -1;
    }

    return listen_sock;
}

static void tcp_server_task_func(void* param)
{
    int socks[TCP_MAX_CONN];
    TickType_t recv_ticks[TCP_MAX_CONN];

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
                    ESP_LOGW(TAG, "Maximum connection count %d reached", TCP_MAX_CONN);
                    close_conn(&sock);
                } else {
                    socks[conn_cout] = sock;
                    recv_ticks[conn_cout] = xTaskGetTickCount();
                    conn_cout++;
                }
            }

            for (int i = 0; i < TCP_MAX_CONN; i++) {
                if (socks[i] > 0) {
                    if (FD_ISSET(socks[i], &read_set)) {
                        uint8_t buf[TCP_BUF_SIZE];

                        int err = rx_poll(socks[i], buf);
                        if (err < 0) {
                            if (shutdown_sem) {
                                xSemaphoreGive(shutdown_sem);
                                vTaskDelete(NULL);
                            }

                            close_conn(&socks[i]);
                        } else if (err > 0) {
                            recv_ticks[i] = xTaskGetTickCount();
                            ESP_LOG_LEVEL(LOG_LVL_DATA, TAG, "Socket (#%d), received buffer length %d", socks[i], err);
                            ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, err, LOG_LVL_DATA);

                            uint16_t len = MODBUS_READ_UINT16(buf, MODBUS_TCP_LEN);
                            if (len == err - 6) {
                                len = modbus_request_exec(&buf[MODBUS_TCP_DATA], len);

                                if (len > 0) {
                                    MODBUS_WRITE_UINT16(buf, MODBUS_TCP_LEN, len);
                                    uint16_t resp_len = len + 6;
                                    ESP_LOG_LEVEL(LOG_LVL_DATA, TAG, "Socket (#%d), write buffer length %d", socks[i], resp_len);
                                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, resp_len, LOG_LVL_DATA);

                                    err = send(socks[i], buf, resp_len, MSG_DONTWAIT);
                                    if (err < 0) {
                                        ESP_LOGE(TAG, "Socket (#%d), fail to send data: errno %d", socks[i], errno);
                                        close_conn(&socks[i]);
                                    }
                                } else {
                                    ESP_LOGW(TAG, "Socket (#%d), no response", socks[i]);
                                }
                            } else {
                                ESP_LOGW(TAG, "Socket (#%d), invalid packet data length", socks[i]);
                            }
                        }
                    } else {
                        TickType_t delta = xTaskGetTickCount() - recv_ticks[i];
                        if (delta > pdMS_TO_TICKS(DISCONNECT_TIMEOUT)) {
                            ESP_LOGW(TAG, "Socket (#%d), closing due inactivity", socks[i]);
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

void tcp_server_start(void)
{
    if (tcp_server_task == NULL) {
        ESP_LOGI(TAG, "Starting server");
        xTaskCreate(tcp_server_task_func, "modbus_tcp_server", 4 * 1024, NULL, 5, &tcp_server_task);
    }
}

void tcp_server_stop(void)
{
    if (tcp_server_task) {
        ESP_LOGI(TAG, "Stopping server");
        shutdown_sem = xSemaphoreCreateBinary();

        close(listen_sock);
        listen_sock = -1;

        if (!xSemaphoreTake(shutdown_sem, pdMS_TO_TICKS(SHUTDOWN_TIMEOUT))) {
            ESP_LOGE(TAG, "Task stop timeout, will be force stoped");
            vTaskDelete(tcp_server_task);
        }

        vSemaphoreDelete(shutdown_sem);
        shutdown_sem = NULL;
        tcp_server_task = NULL;
    }
}

void modbus_tcp_set_enabled(bool enabled)
{
    nvs_set_u8(nvs, NVS_ENABLED, enabled);

    nvs_commit(nvs);

    if (enabled) {
        tcp_server_start();
    } else {
        tcp_server_stop();
    }
}

bool modbus_tcp_is_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}

void modbus_tcp_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    esp_register_shutdown_handler(&tcp_server_stop);

    if (modbus_tcp_is_enabled()) {
        tcp_server_start();
    }
}
