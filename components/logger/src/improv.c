#include "improv.h"

#include <esp_ota_ops.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "sdkconfig.h"

#include "board_config.h"
#include "wifi.h"

#define IMPROV_TYPE_CURRENT_STATE 0x01
#define IMPROV_TYPE_ERROR_STATE   0x02
#define IMPROV_TYPE_RPC           0x03
#define IMPROV_TYPE_RPC_RESPONSE  0x04

#define IMPROV_CMD_WIFI_SETTINGS 0x01
#define IMPROV_CMD_CURRENT_STATE 0x02
#define IMPROV_CMD_DEVICE_INFO   0x03
#define IMPROV_CMD_WIFI_NETWORKS 0x04

#define IMPROV_STATE_STOPPED                0x00
#define IMPROV_STATE_AWAITING_AUTHORIZATION 0x01
#define IMPROV_STATE_READY                  0x02
#define IMPROV_STATE_PROVISIONING           0x03
#define IMPROV_STATE_PROVISIONED            0x04

#define IMPROV_ERROR_INVALID_RPC       0x01
#define IMPROV_ERROR_UNKNOWN_RPC       0x02
#define IMPROV_ERROR_UNABLE_TO_CONNECT 0x03
#define IMPROV_ERROR_NOT_AUTHORIZED    0x04
#define IMPROV_ERROR_UNKNOWN           0xFF

#if CONFIG_IDF_TARGET_ESP32
#define CHIP_FAMILY "ESP32"
#elif CONFIG_IDF_TARGET_ESP32S2
#define CHIP_FAMILY "ESP32-S2"
#elif CONFIG_IDF_TARGET_ESP32S3
#define CHIP_FAMILY "ESP32-S3"
#endif

static const uint8_t IMPROV_HEADER[] = { 'I', 'M', 'P', 'R', 'O', 'V', 0x01 };

static void improv_append_checksum(uint8_t* data, uint16_t data_len)
{
    uint8_t checksum = 0x00;
    for (int i = 0; i < data_len; i++) {
        checksum += data[i];
    }

    data[data_len] = checksum;
}

static void improv_append_str(uint8_t* data, uint8_t* pos, const char* str)
{
    uint8_t len = strlen(str);

    data[*pos] = len;
    memcpy(&data[*pos + 1], str, len);

    *pos += len + 1;
}

static void improv_send_state(int fd, uint8_t state)
{
    uint8_t data[11];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_CURRENT_STATE;
    data[8] = 1;  // length
    data[9] = state;

    improv_append_checksum(data, 10);

    write(fd, data, 11);
    fsync(fd);
}

static void improv_send_error(int fd, uint8_t error)
{
    uint8_t data[11];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_ERROR_STATE;
    data[8] = 1;  // length
    data[9] = error;

    improv_append_checksum(data, 10);

    write(fd, data, 11);
    fsync(fd);
}

static void improv_send_device_url(int fd, uint8_t cmd)
{
    uint8_t data[IMPROV_PACKET_SIZE];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_RPC_RESPONSE;
    data[9] = cmd;

    uint8_t pos = 0;

    char url[64];
    strlcpy(url, "http://", sizeof(url));
    wifi_get_ip(false, &url[7], sizeof(url) - 7);

    improv_append_str(&data[11], &pos, url);

    data[8] = pos + 2;  // total length
    data[10] = pos;     // data length

    improv_append_checksum(data, pos + 11);

    write(fd, data, pos + 12);
    fsync(fd);
}

static void improv_handle_rpc_current_state(int fd)
{
    if (wifi_is_sta_connected()) {
        improv_send_state(fd, IMPROV_STATE_PROVISIONED);
        improv_send_device_url(fd, IMPROV_CMD_CURRENT_STATE);
    } else {
        improv_send_state(fd, IMPROV_STATE_READY);
    }
}

static void improv_handle_rpc_device_info(int fd)
{
    uint8_t data[IMPROV_PACKET_SIZE];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_RPC_RESPONSE;
    data[9] = IMPROV_CMD_DEVICE_INFO;

    uint8_t pos = 0;
    const esp_app_desc_t* app_desc = esp_app_get_description();
    improv_append_str(&data[11], &pos, app_desc->project_name);
    improv_append_str(&data[11], &pos, app_desc->version);
    improv_append_str(&data[11], &pos, CHIP_FAMILY);
    improv_append_str(&data[11], &pos, board_config.device_name);

    data[8] = pos + 2;  // total length
    data[10] = pos;     // data length

    improv_append_checksum(data, pos + 11);

    write(fd, data, pos + 12);
    fsync(fd);
}

static void improv_send_response_wifi_network(int fd, wifi_scan_ap_entry_t* scan_ap)
{
    uint8_t data[IMPROV_PACKET_SIZE];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_RPC_RESPONSE;
    data[9] = IMPROV_CMD_WIFI_NETWORKS;

    uint8_t pos = 0;
    if (scan_ap) {
        improv_append_str(&data[11], &pos, scan_ap->ssid);

        char rssi_str[8];
        snprintf(rssi_str, sizeof(rssi_str), "%d", scan_ap->rssi);
        improv_append_str(&data[11], &pos, rssi_str);

        if (scan_ap->auth) {
            improv_append_str(&data[11], &pos, "YES");
        } else {
            improv_append_str(&data[11], &pos, "NO");
        }
    }
    data[8] = pos + 2;  // total length
    data[10] = pos;     // data length

    improv_append_checksum(data, pos + 11);

    write(fd, data, pos + 12);
    fsync(fd);
}

static void improv_handle_rpc_wifi_networks(int fd)
{
    wifi_scan_ap_list_t* list = wifi_scan_aps();

    wifi_scan_ap_entry_t* entry;
    SLIST_FOREACH (entry, list, entries) {
        improv_send_response_wifi_network(fd, entry);
    }
    improv_send_response_wifi_network(fd, NULL);

    wifi_scan_aps_free(list);
}

static void improv_handle_rpc_wifi_settings(int fd, uint8_t* data, uint8_t data_len)
{
    improv_send_state(fd, IMPROV_STATE_PROVISIONED);
    uint8_t ssid_len = data[2];
    uint8_t password_len = data[3 + ssid_len];

    char ssid[WIFI_SSID_SIZE + 1] = { 0 };
    char password[WIFI_PASSWORD_SIZE + 1] = { 0 };

    if (ssid_len + 3 > data_len || 4 + ssid_len + password_len > data_len) {
        improv_send_error(fd, IMPROV_ERROR_INVALID_RPC);
    } else {
        strncpy(ssid, (char*)&data[3], MIN(ssid_len, WIFI_SSID_SIZE));
        strncpy(password, (char*)&data[4 + ssid_len], MIN(password_len, WIFI_PASSWORD_SIZE));

        if (wifi_set_config(true, ssid, password) == ESP_OK) {
            if (wifi_sta_wait_connect(pdMS_TO_TICKS(10000))) {
                improv_send_state(fd, IMPROV_STATE_PROVISIONED);
                improv_send_device_url(fd, IMPROV_CMD_WIFI_SETTINGS);
            } else {
                improv_send_state(fd, IMPROV_STATE_STOPPED);
                improv_send_error(fd, IMPROV_ERROR_UNABLE_TO_CONNECT);
            }
        } else {
            improv_send_error(fd, IMPROV_ERROR_INVALID_RPC);
        }
    }
}

static void improv_handle_rpc(int fd, uint8_t* data, uint8_t data_len)
{
    switch (data[0]) {
    case IMPROV_CMD_CURRENT_STATE:
        improv_handle_rpc_current_state(fd);
        break;
    case IMPROV_CMD_DEVICE_INFO:
        improv_handle_rpc_device_info(fd);
        break;
    case IMPROV_CMD_WIFI_NETWORKS:
        improv_handle_rpc_wifi_networks(fd);
        break;
    case IMPROV_CMD_WIFI_SETTINGS:
        improv_handle_rpc_wifi_settings(fd, data, data_len);
        break;
    default:
        improv_send_error(fd, IMPROV_ERROR_UNKNOWN_RPC);
        break;
    }
}

void improv_handle(int fd)
{
    uint8_t buf[IMPROV_PACKET_SIZE];

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 250 * 1000,
    };
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);

    if (select(fd + 1, &read_set, NULL, NULL, &tv) > 0) {
        if (FD_ISSET(fd, &read_set)) {
            int readed = read(fd, buf, IMPROV_PACKET_SIZE);
            if (readed > 10) {  // check to minimal improv packet size
                if (memcmp(buf, IMPROV_HEADER, sizeof(IMPROV_HEADER)) == 0) {
                    uint8_t type = buf[7];
                    uint8_t data_len = buf[8];

                    if (8 + data_len + 1 <= readed) {
                        uint8_t checksum = 0x00;
                        for (int i = 0; i < 9 + data_len; i++) {
                            checksum += buf[i];
                        }

                        if (checksum != buf[8 + data_len + 1]) {
                            improv_send_error(fd, IMPROV_ERROR_INVALID_RPC);
                        } else {
                            if (type == IMPROV_TYPE_RPC) {
                                improv_handle_rpc(fd, &buf[9], readed - 8);
                            }
                        }
                    }
                }
            }
        }
    }
}