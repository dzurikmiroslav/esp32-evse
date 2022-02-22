#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_ota_ops.h"

#include "mqtt.h"
#include "json_utils.h"
#include "mqtt.h"
#include "tcp_logger.h"
#include "wifi.h"
#include "timeout_utils.h"
#include "evse.h"
#include "board_config.h"
#include "energy_meter.h"
#include "cable_lock.h"
#include "aux.h"
#include "serial.h"
#include "proximity.h"

#define RETURN_ON_ERROR(x) do {                 \
        esp_err_t err_rc_ = (x);                \
        if (unlikely(err_rc_ != ESP_OK)) {      \
            return err_rc_;                     \
        }                                       \
    } while(0)

cJSON* json_get_evse_config(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "chargingCurrent", evse_get_chaging_current() / 10.0);
    cJSON_AddNumberToObject(root, "defaultChargingCurrent", evse_get_default_chaging_current() / 10.0);
    cJSON_AddBoolToObject(root, "requireAuth", evse_is_require_auth());
    cJSON_AddBoolToObject(root, "proximityCheck", proximity_is_enabled());

    cJSON_AddStringToObject(root, "cableLock", cable_lock_type_to_str(cable_lock_get_type()));

    cJSON_AddStringToObject(root, "energyMeter", energy_meter_mode_to_str(energy_meter_get_mode()));
    cJSON_AddNumberToObject(root, "acVoltage", energy_meter_get_ac_voltage());
    cJSON_AddNumberToObject(root, "pulseAmount", energy_meter_get_pulse_amount());

    cJSON_AddStringToObject(root, "aux1", aux_mode_to_str(aux_get_mode(AUX_ID_1)));
    cJSON_AddStringToObject(root, "aux2", aux_mode_to_str(aux_get_mode(AUX_ID_2)));
    cJSON_AddStringToObject(root, "aux3", aux_mode_to_str(aux_get_mode(AUX_ID_3)));

    return root;
}

esp_err_t json_set_evse_config(cJSON* root)
{
    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "chargingCurrent"))) {
        RETURN_ON_ERROR(evse_set_chaging_current(cJSON_GetObjectItem(root, "chargingCurrent")->valuedouble * 10));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "defaultChargingCurrent"))) {
        RETURN_ON_ERROR(evse_set_default_chaging_current(cJSON_GetObjectItem(root, "defaultChargingCurrent")->valuedouble * 10));
    }
    if (cJSON_IsBool(cJSON_GetObjectItem(root, "requireAuth"))) {
        evse_set_require_auth(cJSON_IsTrue(cJSON_GetObjectItem(root, "requireAuth")));
    }
    if (cJSON_IsBool(cJSON_GetObjectItem(root, "proximityCheck"))) {
        RETURN_ON_ERROR(proximity_set_enabled(cJSON_IsTrue(cJSON_GetObjectItem(root, "proximityCheck"))));
    }

    if (cJSON_IsString(cJSON_GetObjectItem(root, "cableLock"))) {
        RETURN_ON_ERROR(cable_lock_set_type(cable_lock_str_to_type(cJSON_GetObjectItem(root, "cableLock")->valuestring)));
    }

    if (cJSON_IsString(cJSON_GetObjectItem(root, "energyMeter"))) {
        RETURN_ON_ERROR(energy_meter_set_mode(energy_meter_str_to_mode(cJSON_GetObjectItem(root, "energyMeter")->valuestring)));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "acVoltage"))) {
        RETURN_ON_ERROR(energy_meter_set_ac_voltage(cJSON_GetObjectItem(root, "acVoltage")->valuedouble));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "pulseAmount"))) {
        RETURN_ON_ERROR(energy_meter_set_pulse_amount(cJSON_GetObjectItem(root, "pulseAmount")->valuedouble));
    }

    if (cJSON_IsString(cJSON_GetObjectItem(root, "aux1"))) {
        RETURN_ON_ERROR(aux_set_mode(AUX_ID_1, aux_str_to_mode(cJSON_GetObjectItem(root, "aux1")->valuestring)));
    }
    if (cJSON_IsString(cJSON_GetObjectItem(root, "aux2"))) {
        RETURN_ON_ERROR(aux_set_mode(AUX_ID_2, aux_str_to_mode(cJSON_GetObjectItem(root, "aux2")->valuestring)));
    }
    if (cJSON_IsString(cJSON_GetObjectItem(root, "aux3"))) {
        RETURN_ON_ERROR(aux_set_mode(AUX_ID_3, aux_str_to_mode(cJSON_GetObjectItem(root, "aux3")->valuestring)));
    }

    return ESP_OK;
}

cJSON* json_get_wifi_config(void)
{
    cJSON* root = cJSON_CreateObject();

    char str[32];

    cJSON_AddBoolToObject(root, "enabled", wifi_get_enabled());
    wifi_get_ssid(str);
    cJSON_AddStringToObject(root, "ssid", str);

    return root;
}

esp_err_t json_set_wifi_config(cJSON* root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "enabled"));
    char* ssid = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ssid"));
    char* password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));

    return timeout_set_wifi_config(enabled, ssid, password);
}

cJSON* json_get_mqtt_config(void)
{
    cJSON* root = cJSON_CreateObject();

    char str[64];

    cJSON_AddBoolToObject(root, "enabled", mqtt_get_enabled());
    mqtt_get_server(str);
    cJSON_AddStringToObject(root, "server", str);
    mqtt_get_base_topic(str);
    cJSON_AddStringToObject(root, "baseTopic", str);
    mqtt_get_user(str);
    cJSON_AddStringToObject(root, "user", str);
    mqtt_get_password(str);
    cJSON_AddStringToObject(root, "password", str);
    cJSON_AddNumberToObject(root, "periodicity", mqtt_get_periodicity());

    return root;
}

esp_err_t json_set_mqtt_config(cJSON* root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "enabled"));
    char* server = cJSON_GetStringValue(cJSON_GetObjectItem(root, "server"));
    char* base_topic = cJSON_GetStringValue(cJSON_GetObjectItem(root, "baseTopic"));
    char* user = cJSON_GetStringValue(cJSON_GetObjectItem(root, "user"));
    char* password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));
    uint16_t periodicity = cJSON_GetNumberValue(cJSON_GetObjectItem(root, "periodicity"));

    return mqtt_set_config(enabled, server, base_topic, user, password, periodicity);
}


cJSON* json_get_serial_config(void)
{
    cJSON* root = cJSON_CreateObject();

    char mode_name[16];
    char baud_rate_name[16];
    char data_bits_name[16];
    char stop_bits_name[16];
    char parity_name[16];

    for (serial_id_t id = 0; id < SERIAL_ID_MAX; id++) {
        if (serial_is_available(id)) {
            sprintf(mode_name, "serial%dMode", id + 1);
            sprintf(baud_rate_name, "serial%dBaudRate", id + 1);
            sprintf(data_bits_name, "serial%dDataBits", id + 1);
            sprintf(stop_bits_name, "serial%dStopBits", id + 1);
            sprintf(parity_name, "serial%dParity", id + 1);

            cJSON_AddStringToObject(root, mode_name, serial_mode_to_str(serial_get_mode(id)));
            cJSON_AddNumberToObject(root, baud_rate_name, serial_get_baud_rate(id));
            cJSON_AddStringToObject(root, data_bits_name, serial_data_bits_to_str(serial_get_data_bits(id)));
            cJSON_AddStringToObject(root, stop_bits_name, serial_stop_bits_to_str(serial_get_stop_bits(id)));
            cJSON_AddStringToObject(root, parity_name, serial_parity_to_str(serial_get_parity(id)));
        }
    }

    return root;
}

esp_err_t json_set_serial_config(cJSON* root)
{
    char mode_name[16];
    char baud_rate_name[16];
    char data_bits_name[16];
    char stop_bits_name[16];
    char parity_name[16];

    for (serial_id_t id = 0; id < SERIAL_ID_MAX; id++) {
        if (serial_is_available(id)) {
            sprintf(mode_name, "serial%dMode", id + 1);
            sprintf(baud_rate_name, "serial%dBaudRate", id + 1);
            sprintf(data_bits_name, "serial%dDataBits", id + 1);
            sprintf(stop_bits_name, "serial%dStopBits", id + 1);
            sprintf(parity_name, "serial%dParity", id + 1);

            if (cJSON_IsString(cJSON_GetObjectItem(root, mode_name))) {
                serial_mode_t mode = serial_str_to_mode(cJSON_GetObjectItem(root, mode_name)->valuestring);
                int baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(root, baud_rate_name));
                uart_word_length_t data_bits = serial_str_to_data_bits(cJSON_GetStringValue(cJSON_GetObjectItem(root, data_bits_name)));
                uart_word_length_t stop_bits = serial_str_to_stop_bits(cJSON_GetStringValue(cJSON_GetObjectItem(root, stop_bits_name)));
                uart_parity_t parity = serial_str_to_parity(cJSON_GetStringValue(cJSON_GetObjectItem(root, parity_name)));

                RETURN_ON_ERROR(serial_set_config(id, baud_rate, data_bits, stop_bits, parity));
                RETURN_ON_ERROR(serial_set_mode(id, mode));
            }
        }
    }

    return ESP_OK;
}

cJSON* json_get_tcp_logger_config(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "enabled", tcp_logger_get_enabled());
    cJSON_AddNumberToObject(root, "port", tcp_logger_get_port());

    return root;
}

esp_err_t json_set_tcp_logger_config(cJSON* root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "enabled"));
    uint16_t port = cJSON_GetObjectItem(root, "port")->valuedouble;

    return tcp_logger_set_config(enabled, port);
}

cJSON* json_get_state(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "enabled", evse_get_state() != EVSE_STATE_DISABLED);
    cJSON_AddBoolToObject(root, "error", evse_get_state() == EVSE_STATE_ERROR);
    cJSON_AddBoolToObject(root, "session", evse_state_is_session(evse_get_state()));
    cJSON_AddBoolToObject(root, "charging", evse_state_is_charging(evse_get_state()));
    cJSON_AddBoolToObject(root, "paused", evse_is_paused());
    cJSON_AddBoolToObject(root, "pendingAuth", evse_is_pending_auth());
    cJSON_AddNumberToObject(root, "elapsed", energy_meter_get_session_elapsed());
    cJSON_AddNumberToObject(root, "consumption", energy_meter_get_session_consumption());
    cJSON_AddNumberToObject(root, "actualPower", energy_meter_get_power());
    float values[3];
    energy_meter_get_voltage(values);
    cJSON_AddItemToObject(root, "voltage", cJSON_CreateFloatArray(values, 3));
    energy_meter_get_current(values);
    cJSON_AddItemToObject(root, "current", cJSON_CreateFloatArray(values, 3));

    return root;
}

cJSON* json_get_info(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "uptime", esp_timer_get_time() / 1000000);
    const esp_app_desc_t* app_desc = esp_ota_get_app_description();
    cJSON_AddStringToObject(root, "appVersion", app_desc->version);
    cJSON_AddStringToObject(root, "appDate", app_desc->date);
    cJSON_AddStringToObject(root, "appTime", app_desc->time);
    cJSON_AddStringToObject(root, "idfVersion", app_desc->idf_ver);
    cJSON_AddStringToObject(root, "chip", CONFIG_IDF_TARGET);
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    cJSON_AddNumberToObject(root, "chipCores", chip_info.cores);
    cJSON_AddNumberToObject(root, "chipRevision", chip_info.revision);
    uint8_t mac[6];
    char str[32];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    sprintf(str, MACSTR, MAC2STR(mac));
    cJSON_AddStringToObject(root, "mac", str);
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, str, sizeof(str));
    cJSON_AddStringToObject(root, "ip", str);

    return root;
}

static const char* serial_to_str(board_config_serial_t serial)
{
    switch (serial)
    {
    case BOARD_CONFIG_SERIAL_UART:
        return "uart";
    case BOARD_CONFIG_SERIAL_RS485:
        return "rs485";
    default:
        return "none";
    }
}

cJSON* json_get_board_config(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "maxChargingCurrent", board_config.max_charging_current);
    cJSON_AddBoolToObject(root, "cableLock", board_config.cable_lock);
    switch (board_config.energy_meter) {
    case BOARD_CONFIG_ENERGY_METER_CUR:
        cJSON_AddStringToObject(root, "energyMeter", "cur");
        break;
    case BOARD_CONFIG_ENERGY_METER_CUR_VLT:
        cJSON_AddStringToObject(root, "energyMeter", "cur_vlt");
        break;
    default:
        cJSON_AddStringToObject(root, "energyMeter", "none");
    }
    cJSON_AddBoolToObject(root, "energyMeterThreePhases", board_config.energy_meter_three_phases);

    cJSON_AddBoolToObject(root, "aux1", board_config.aux_1);
    cJSON_AddBoolToObject(root, "aux2", board_config.aux_2);
    cJSON_AddBoolToObject(root, "aux3", board_config.aux_3);

    cJSON_AddStringToObject(root, "serial1", serial_to_str(board_config.serial_1));
    cJSON_AddStringToObject(root, "serial2", serial_to_str(board_config.serial_2));
#if SOC_UART_NUM > 2
    cJSON_AddStringToObject(root, "serial3", serial_to_str(board_config.serial_3));
#else
    cJSON_AddStringToObject(root, "serial3", serial_to_str(BOARD_CONFIG_SERIAL_NONE));
#endif
    return root;
}
