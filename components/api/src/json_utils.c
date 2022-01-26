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


const char* sw_mode_to_str(aux_mode_t mode) {
    switch (mode)
    {
    case AUX_MODE_PAUSE_BUTTON:
        return "pause_button";
    case AUX_MODE_PAUSE_SWITCH:
        return "pause_switch";
    case AUX_MODE_UNPAUSE_SWITCH:
        return "unpause_switch";
    case AUX_MODE_AUTHORIZE_BUTTON:
        return "authorize_button";
    case AUX_MODE_PULSE_ENERGY_METER:
        return "pulse_energy_meter";
    default:
        return "none";
    }
}

const char* em_mode_to_str(energy_meter_mode_t mode) {
    switch (mode)
    {
    case ENERGY_METER_MODE_CUR:
        return "cur";
    case ENERGY_METER_MODE_CUR_VLT:
        return "cur_vlt";
    case ENERGY_METER_MODE_PULSE:
        return "pulse";
    default:
        return "none";
    }
}

const char* cl_type_to_str(cable_lock_type_t type) {
    switch (type)
    {
    case CABLE_LOCK_TYPE_MOTOR:
        return "motor";
    case CABLE_LOCK_TYPE_SOLENOID:
        return "solenoid";
    default:
        return "none";
    }
}

cJSON* json_get_evse_config(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "chargingCurrent", evse_get_chaging_current() / 10.0);
    cJSON_AddNumberToObject(root, "defaultChargingCurrent", evse_get_default_chaging_current() / 10.0);
    cJSON_AddBoolToObject(root, "requireAuth", evse_is_require_auth());

    cJSON_AddStringToObject(root, "cableLock", cl_type_to_str(cable_lock_get_type()));

    cJSON_AddStringToObject(root, "energyMeter", em_mode_to_str(energy_meter_get_mode()));
    cJSON_AddNumberToObject(root, "acVoltage", energy_meter_get_ac_voltage());
    cJSON_AddNumberToObject(root, "pulseAmount", energy_meter_get_pulse_amount());

    cJSON_AddStringToObject(root, "aux1", sw_mode_to_str(aux_get_mode(AUX_ID_1)));
    cJSON_AddStringToObject(root, "aux2", sw_mode_to_str(aux_get_mode(AUX_ID_2)));
    cJSON_AddStringToObject(root, "aux3", sw_mode_to_str(aux_get_mode(AUX_ID_3)));

    return root;
}

aux_mode_t str_to_sw_mode(const char* string) {
    if (!strcmp(string, "pause_button")) {
        return AUX_MODE_PAUSE_BUTTON;
    }
    if (!strcmp(string, "pause_switch")) {
        return AUX_MODE_PAUSE_SWITCH;
    }
    if (!strcmp(string, "unpause_switch")) {
        return AUX_MODE_UNPAUSE_SWITCH;
    }
    if (!strcmp(string, "authorize_button")) {
        return AUX_MODE_AUTHORIZE_BUTTON;
    }
    if (!strcmp(string, "pulse_energy_meter")) {
        return AUX_MODE_PULSE_ENERGY_METER;
    }
    return AUX_MODE_NONE;
}

energy_meter_mode_t str_to_em_mode(const char* string) {
    if (!strcmp(string, "cur")) {
        return ENERGY_METER_MODE_CUR;
    }
    if (!strcmp(string, "cur_vlt")) {
        return ENERGY_METER_MODE_CUR_VLT;
    }
    if (!strcmp(string, "pulse")) {
        return ENERGY_METER_MODE_PULSE;
    }
    return ENERGY_METER_MODE_NONE;
}

cable_lock_type_t str_to_cl_type(const char* string) {
    if (!strcmp(string, "motor")) {
        return CABLE_LOCK_TYPE_MOTOR;
    }
    if (!strcmp(string, "solenoid")) {
        return CABLE_LOCK_TYPE_SOLENOID;
    }
    return CABLE_LOCK_TYPE_NONE;
}

void json_set_evse_config(cJSON* root)
{
    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "chargingCurrent"))) {
        evse_set_chaging_current(cJSON_GetObjectItem(root, "chargingCurrent")->valuedouble * 10);
    }

    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "defaultChargingCurrent"))) {
        evse_set_default_chaging_current(cJSON_GetObjectItem(root, "defaultChargingCurrent")->valuedouble * 10);
    }

    if (cJSON_IsBool(cJSON_GetObjectItem(root, "requireAuth"))) {
        evse_set_require_auth(cJSON_IsTrue(cJSON_GetObjectItem(root, "requireAuth")));
    }

    if (cJSON_IsString(cJSON_GetObjectItem(root, "cableLock"))) {
        cable_lock_set_type(str_to_cl_type(cJSON_GetObjectItem(root, "cableLock")->valuestring));
    }

    if (cJSON_IsString(cJSON_GetObjectItem(root, "energyMeter"))) {
        energy_meter_set_mode(str_to_em_mode(cJSON_GetObjectItem(root, "energyMeter")->valuestring));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "acVoltage"))) {
        energy_meter_set_ac_voltage(cJSON_GetObjectItem(root, "acVoltage")->valuedouble);
    }
    if (cJSON_IsNumber(cJSON_GetObjectItem(root, "pulseAmount"))) {
        energy_meter_set_pulse_amount(cJSON_GetObjectItem(root, "pulseAmount")->valuedouble);
    }

    if (cJSON_IsString(cJSON_GetObjectItem(root, "aux1"))) {
        aux_set_mode(AUX_ID_1, str_to_sw_mode(cJSON_GetObjectItem(root, "aux1")->valuestring));
    }
    if (cJSON_IsString(cJSON_GetObjectItem(root, "aux2"))) {
        aux_set_mode(AUX_ID_2, str_to_sw_mode(cJSON_GetObjectItem(root, "aux2")->valuestring));
    }
    if (cJSON_IsString(cJSON_GetObjectItem(root, "aux3"))) {
        aux_set_mode(AUX_ID_3, str_to_sw_mode(cJSON_GetObjectItem(root, "aux3")->valuestring));
    }
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

void json_set_wifi_config(cJSON* root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "enabled"));
    char* ssid = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ssid"));
    char* password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));

    timeout_set_wifi_config(enabled, ssid, password);
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

void json_set_mqtt_config(cJSON* root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "enabled"));
    char* server = cJSON_GetStringValue(cJSON_GetObjectItem(root, "server"));
    char* base_topic = cJSON_GetStringValue(cJSON_GetObjectItem(root, "baseTopic"));
    char* user = cJSON_GetStringValue(cJSON_GetObjectItem(root, "user"));
    char* password = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));
    uint16_t periodicity = cJSON_GetObjectItem(root, "periodicity")->valuedouble;

    mqtt_set_config(enabled, server, base_topic, user, password, periodicity);
}


cJSON* json_get_tcp_logger_config(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "enabled", tcp_logger_get_enabled());
    cJSON_AddNumberToObject(root, "port", tcp_logger_get_port());

    return root;
}

void json_set_tcp_logger_config(cJSON* root)
{
    bool enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "enabled"));
    uint16_t port = cJSON_GetObjectItem(root, "port")->valuedouble;

    tcp_logger_set_config(enabled, port);
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

    return root;
}
