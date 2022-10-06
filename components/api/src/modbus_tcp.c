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
#include <stdint.h>

#include "modbus_tcp.h"
#include "modbus.h"
#include "modbus_utils.h"


#define TCP_PORT                502
#define TCP_MAX_CONN            3
#define TCP_LISTEN_BACKLOG      5
#define TCP_BUF_SIZE            (MODBUS_PACKET_SIZE + 7)

#define RESPONSE_TIMEOUT        999
#define DISCONNECT_TIMEOUT      1000
#define SHUTDOWN_TIMEOUT        1000

#define MODBUS_TCP_TID          0
#define MODBUS_TCP_PID          2
#define MODBUS_TCP_LEN          4
#define MODBUS_TCP_DATA         6

static const char* TAG = "modbus_tcp";

static int listen_sock = -1;

static TaskHandle_t tcp_server_task = NULL;

static SemaphoreHandle_t shutdown_sem = NULL;

QueueHandle_t modbus_tcp_req_queue = NULL;

QueueHandle_t modbus_tcp_res_queue = NULL;

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

static int tx_resp(int sock, uint8_t* buf, uint16_t len)
{
    fd_set xWriteSet;
    fd_set xErrorSet;
    int ret = 0;

    FD_ZERO(&xWriteSet);
    FD_ZERO(&xErrorSet);
    FD_SET(sock, &xWriteSet);
    FD_SET(sock, &xErrorSet);

    ret = select(sock + 1, NULL, &xWriteSet, &xErrorSet, NULL);
    if ((ret == -1) || FD_ISSET(sock, &xErrorSet)) {
        ESP_LOGE(TAG, "Socket (#%d), send select() error: %d", sock, errno);
    } else {
        ret = send(sock, buf, len, MSG_DONTWAIT);
        if (ret < 0) {
            ESP_LOGE(TAG, "Socket (#%d), fail to send data: errno %d", sock, errno);
        }
    }

    return ret;
}

static void close_socket(int* sock)
{
    shutdown(*sock, 0);
    close(*sock);
    *sock = -1;
}

static int port_bind(void)
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0); //TODO IPPROTO_UDP IPPROTO_TCP
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

    int sock = accept(listen_sock, (struct sockaddr*)&source_addr, &addr_len);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to accept connection: errno=%d", errno);
        close(sock);
    } else {
        inet_ntoa_r(((struct sockaddr_in*)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGD(TAG, "Socket (#%d) accepted ip address: %s", sock, addr_str);
    }

    return sock;
}

static void tcp_server_task_func(void* param)
{
    int socks[TCP_MAX_CONN];
    TickType_t recv_ticks[TCP_MAX_CONN];
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
                    socks[conn_cout] = sock;
                    recv_ticks[conn_cout] = xTaskGetTickCount();
                    conn_cout++;
                }
            }

            for (int i = 0; i < TCP_MAX_CONN; i++) {
                if (socks[i] > 0) {
                    if (fds[i + 1].revents & POLLIN) {
                        uint8_t buf[TCP_BUF_SIZE];
                        modbus_packet_t packet;
                        int err = rx_poll(socks[i], buf);
                        if (err < 0) {
                            if (shutdown_sem) {
                                xSemaphoreGive(shutdown_sem);
                                vTaskDelete(NULL);
                            }

                            ESP_LOGD(TAG, "Socket (#%d), closing", socks[i]);
                            close_socket(&socks[i]);
                        } else if (err > 0) {
                            recv_ticks[i] = xTaskGetTickCount();
                            ESP_LOGD(TAG, "Socket (#%d), received buffer length %d", socks[i], err);
                            ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, err, ESP_LOG_DEBUG);

                            packet.len = MODBUS_READ_UINT16(buf, MODBUS_TCP_LEN);
                            if (packet.len == err - 6) {
                                memcpy(packet.data, &buf[MODBUS_TCP_DATA], packet.len);

                                xQueueSend(modbus_tcp_req_queue, &packet, portMAX_DELAY);
                                if (xQueueReceive(modbus_tcp_res_queue, &packet, pdMS_TO_TICKS(RESPONSE_TIMEOUT))) {
                                    MODBUS_WRITE_UINT16(buf, MODBUS_TCP_LEN, packet.len);
                                    memcpy(&buf[MODBUS_TCP_DATA], packet.data, packet.len);

                                    uint16_t resp_len = packet.len + 6;
                                    ESP_LOGD(TAG, "Socket (#%d), write buffer length %d", socks[i], resp_len);
                                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, resp_len, ESP_LOG_DEBUG);

                                    if (tx_resp(socks[i], buf, resp_len) < 0) {
                                        close_socket(&socks[i]);
                                    }
                                } else {
                                    ESP_LOGW(TAG, "Socket (#%d), response time exceeds", socks[i]);
                                }
                            } else {
                                ESP_LOGW(TAG, "Socket (#%d), invalid packet data length", socks[i]);
                            }
                        }
                    } else {
                        TickType_t delta = xTaskGetTickCount() - recv_ticks[i];
                        if (delta > pdMS_TO_TICKS(DISCONNECT_TIMEOUT)) {
                            ESP_LOGW(TAG, "Socket (#%d), closing due inactivity", socks[i]);
                            close_socket(&socks[i]);
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

void modbus_tcp_stop(void)
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

void modbus_tcp_listen(void)
{
    if (tcp_server_task == NULL) {
        ESP_LOGI(TAG, "Starting server");
        xTaskCreate(tcp_server_task_func, "modbus_tcp_server", 4096, NULL, 5, &tcp_server_task);
    }
}

void modbus_tcp_init(void)
{
    modbus_tcp_req_queue = xQueueCreate(1, sizeof(modbus_packet_t));
    modbus_tcp_res_queue = xQueueCreate(5, sizeof(modbus_packet_t));

    esp_register_shutdown_handler(&modbus_tcp_stop);
}
