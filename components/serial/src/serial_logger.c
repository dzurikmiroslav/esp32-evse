#include "serial_logger.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <string.h>
#include <sys/param.h>

#include "sdkconfig.h"

#include "board_config.h"
#include "logger.h"
#include "wifi.h"

#define IMPROV_PACKET_SIZE 266

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

static const char* TAG = "serial_logger";

static uart_port_t port = -1;

static TaskHandle_t serial_logger_task = NULL;

static const uint8_t IMPROV_HEADER[] = { 'I', 'M', 'P', 'R', 'O', 'V', 0x01 };

static void improv_append_checksum(uint8_t* data, uint16_t data_len)
{
    uint8_t checksum = 0x00;
    for (uint16_t i = 0; i < data_len; i++) {
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

static void improv_send_state(uint8_t state)
{
    uint8_t data[11];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_CURRENT_STATE;
    data[8] = 1;  // length
    data[9] = state;

    improv_append_checksum(data, 10);

    uart_write_bytes(port, data, 11);
    uart_wait_tx_done(port, portMAX_DELAY);
}

static void improv_send_error(uint8_t error)
{
    uint8_t data[11];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_ERROR_STATE;
    data[8] = 1;  // length
    data[9] = error;

    improv_append_checksum(data, 10);

    uart_write_bytes(port, data, 11);
    uart_wait_tx_done(port, portMAX_DELAY);
}

static void improv_send_device_url(uint8_t cmd)
{
    uint8_t data[IMPROV_PACKET_SIZE];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_RPC_RESPONSE;
    data[9] = cmd;

    uint8_t pos = 0;

    char url[64];
    strcpy(url, "http://");
    wifi_get_ip(false, &url[7]);

    improv_append_str(&data[11], &pos, url);

    data[8] = pos + 2;  // total length
    data[10] = pos;     // data length

    improv_append_checksum(data, pos + 11);

    uart_write_bytes(port, data, pos + 12);
    uart_wait_tx_done(port, portMAX_DELAY);
}

static void improv_handle_rpc_current_state(void)
{
    if (xEventGroupGetBits(wifi_event_group) & WIFI_STA_CONNECTED_BIT) {
        improv_send_state(IMPROV_STATE_PROVISIONED);
        improv_send_device_url(IMPROV_CMD_CURRENT_STATE);
    } else {
        improv_send_state(IMPROV_STATE_READY);
    }
}

static void improv_handle_rpc_device_info(void)
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

    uart_write_bytes(port, data, pos + 12);
    uart_wait_tx_done(port, portMAX_DELAY);
}

static void improv_send_response_wifi_network(wifi_scan_ap_entry_t* scan_ap)
{
    uint8_t data[IMPROV_PACKET_SIZE];
    memcpy(data, IMPROV_HEADER, sizeof(IMPROV_HEADER));
    data[7] = IMPROV_TYPE_RPC_RESPONSE;
    data[9] = IMPROV_CMD_WIFI_NETWORKS;

    uint8_t pos = 0;
    if (scan_ap) {
        improv_append_str(&data[11], &pos, scan_ap->ssid);

        char rssi_str[8];
        sprintf(rssi_str, "%d", scan_ap->rssi);
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

    uart_write_bytes(port, data, pos + 12);
    uart_wait_tx_done(port, portMAX_DELAY);
}

static void improv_handle_rpc_wifi_networks(void)
{
    wifi_scan_ap_list_t* list = wifi_scan_aps();

    wifi_scan_ap_entry_t* entry;
    SLIST_FOREACH (entry, list, entries) {
        improv_send_response_wifi_network(entry);
    }
    improv_send_response_wifi_network(NULL);

    wifi_scan_aps_free(list);
}

static void improv_handle_rpc_wifi_settings(uint8_t* data, uint8_t data_len)
{
    improv_send_state(IMPROV_STATE_PROVISIONED);
    uint8_t ssid_len = data[2];
    uint8_t password_len = data[3 + ssid_len];

    char ssid[WIFI_SSID_SIZE + 1] = { 0 };
    char password[WIFI_PASSWORD_SIZE + 1] = { 0 };

    if (ssid_len + 3 > data_len || 4 + ssid_len + password_len > data_len) {
        improv_send_error(IMPROV_ERROR_INVALID_RPC);
    } else {
        strncpy(ssid, (char*)&data[3], MIN(ssid_len, WIFI_SSID_SIZE));
        strncpy(password, (char*)&data[4 + ssid_len], MIN(password_len, WIFI_PASSWORD_SIZE));

        if (wifi_set_config(true, ssid, password) == ESP_OK) {
            if (xEventGroupWaitBits(wifi_event_group, WIFI_STA_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000)) & WIFI_STA_CONNECTED_BIT) {
                improv_send_state(IMPROV_STATE_PROVISIONED);
                improv_send_device_url(IMPROV_CMD_WIFI_SETTINGS);
            } else {
                improv_send_state(IMPROV_STATE_STOPPED);
                improv_send_error(IMPROV_ERROR_UNABLE_TO_CONNECT);
            }
        } else {
            improv_send_error(IMPROV_ERROR_INVALID_RPC);
        }
    }
}

static void improv_handle_rpc(uint8_t* data, uint8_t data_len)
{
    switch (data[0]) {
    case IMPROV_CMD_CURRENT_STATE:
        improv_handle_rpc_current_state();
        break;
    case IMPROV_CMD_DEVICE_INFO:
        improv_handle_rpc_device_info();
        break;
    case IMPROV_CMD_WIFI_NETWORKS:
        improv_handle_rpc_wifi_networks();
        break;
    case IMPROV_CMD_WIFI_SETTINGS:
        improv_handle_rpc_wifi_settings(data, data_len);
        break;
    default:
        improv_send_error(IMPROV_ERROR_UNKNOWN_RPC);
        break;
    }
}

static void serial_logger_task_func(void* param)
{
    char* str;
    uint16_t str_len;
    uint16_t index = 0;
    uint8_t buf[IMPROV_PACKET_SIZE];
    while (true) {
        int len = uart_read_bytes(port, buf, IMPROV_PACKET_SIZE, pdMS_TO_TICKS(250));
        if (len > 10) {  // check to minimal improv packet size
            if (memcmp(buf, IMPROV_HEADER, sizeof(IMPROV_HEADER)) == 0) {
                uint8_t type = buf[7];
                uint8_t data_len = buf[8];

                if (8 + data_len + 1 <= len) {
                    uint8_t checksum = 0x00;
                    for (uint8_t i = 0; i < 9 + data_len; i++) {
                        checksum += buf[i];
                    }

                    if (checksum != buf[8 + data_len + 1]) {
                        improv_send_error(IMPROV_ERROR_INVALID_RPC);
                    } else {
                        if (type == IMPROV_TYPE_RPC) {
                            improv_handle_rpc(&buf[9], len - 8);
                        }
                    }
                }
            }
        }

        if (xEventGroupWaitBits(logger_event_group, LOGGER_SERIAL_BIT, pdTRUE, pdFALSE, 0)) {
            while (logger_read(&index, &str, &str_len)) {
                uart_write_bytes(port, str, str_len);
                uart_wait_tx_done(port, portMAX_DELAY);
            }
        }
    }
}

void serial_logger_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485)
{
    ESP_LOGI(TAG, "Starting on uart %d", uart_num);

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bit,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config() returned 0x%x", err);
        return;
    }

    err = uart_driver_install(uart_num, IMPROV_PACKET_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install() returned 0x%x", err);
        return;
    }
    port = uart_num;

    if (rs485) {
        err = uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_set_mode() returned 0x%x", err);
            return;
        }
        err = uart_set_rx_timeout(uart_num, 3);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "uart_set_rx_timeout() returned 0x%x", err);
            return;
        }
    }

    xTaskCreate(serial_logger_task_func, "serial_logger_task", 4 * 1024, NULL, 5, &serial_logger_task);
}

void serial_logger_stop(void)
{
    ESP_LOGI(TAG, "Stopping");

    if (serial_logger_task) {
        vTaskDelete(serial_logger_task);
        serial_logger_task = NULL;
    }

    if (port != -1) {
        uart_driver_delete(port);
        port = -1;
    }
}