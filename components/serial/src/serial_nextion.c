#include "serial_nextion.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <string.h>
#include <time.h>

#include "board_config.h"
#include "energy_meter.h"
#include "evse.h"
#include "temp_sensor.h"
#include "wifi.h"

#define BUF_SIZE 256

#define NEX_RET_BUF_OVERFLOW 0x24
#define NEX_RET_AUTO_SLEEP   0x86
#define NEX_RET_READY        0x88

static const char* VAR_STATE = "state";
static const char* VAR_ENABLED = "en";
static const char* VAR_ERROR = "err";
static const char* VAR_PENDING_AUTH = "pendAuth";
static const char* VAR_LIMIT_REACHED = "limReach";
static const char* VAR_MAX_CHARGING_CURRENT = "maxChCur";
static const char* VAR_CHARGING_CURRENT = "chCur";
static const char* VAR_DEFAULT_CHARGING_CURRENT = "defChCur";
static const char* VAR_SESSION_TIME = "sesTime";
static const char* VAR_CHARGING_TIME = "chTime";
static const char* VAR_POWER = "power";
static const char* VAR_CONSUMPTION = "consum";
static const char* VAR_TOTAL_CONSUMPTION = "totConsum";
static const char* VAR_VOLTAGE_L1 = "vltL1";
static const char* VAR_VOLTAGE_L2 = "vltL2";
static const char* VAR_VOLTAGE_L3 = "vltL3";
static const char* VAR_CURRENT_L1 = "curL1";
static const char* VAR_CURRENT_L2 = "curL2";
static const char* VAR_CURRENT_L3 = "curL3";
static const char* VAR_CONSUMPTION_LIMIT = "consumLim";
static const char* VAR_CHARGING_TIME_LIMIT = "chTimeLim";
static const char* VAR_UNDER_POWER_LIMIT = "uPowerLim";
static const char* VAR_DEFAULT_CONSUMPTION_LIMIT = "defConsumLim";
static const char* VAR_DEFAULT_CHARGING_TIME_LIMIT = "defChTimeLim";
static const char* VAR_DEFAULT_UNDER_POWER_LIMIT = "defUPowerLim";
static const char* VAR_DEVICE_NAME = "devName";
static const char* VAR_UPTIME = "uptime";
static const char* VAR_TEMPERATURE = "temp";  // TODO deprecated, same as VAR_HIGH_TEMPERATURE
static const char* VAR_LOW_TEMPERATURE = "loTemp";
static const char* VAR_HIGH_TEMPERATURE = "hiTemp";
static const char* VAR_IP = "ip";
static const char* VAR_APP_VERSION = "appVer";
static const char* VAR_HEAP_SIZE = "heap";
static const char* VAR_MAX_HEAP_SIZE = "maxHeap";

static const char* VAR_FMT_STATE = "state.txt=\"%s\"";
static const char* VAR_FMT_ENABLED = "en.val=%" PRIu8;
static const char* VAR_FMT_ERROR = "err.val=%" PRIu16;
static const char* VAR_FMT_PENDING_AUTH = "pendAuth.val=%" PRIu8;
static const char* VAR_FMT_LIMIT_REACHED = "limReach.val=%" PRIu8;
static const char* VAR_FMT_MAX_CHARGING_CURRENT = "maxChCur.val=%" PRIu8;
static const char* VAR_FMT_CHARGING_CURRENT = "chCur.val=%" PRIu16;
static const char* VAR_FMT_DEFAULT_CHARGING_CURRENT = "defChCur.val=%" PRIu16;
static const char* VAR_FMT_SESSION_TIME = "sesTime.val=%" PRIu32;
static const char* VAR_FMT_CHARGING_TIME = "chTime.val=%" PRIu32;
static const char* VAR_FMT_POWER = "power.val=%" PRIu16;
static const char* VAR_FMT_CONSUMPTION = "consum.val=%" PRIu32;
static const char* VAR_FMT_TOTAL_CONSUMPTION = "totConsum.val=%" PRIu64;
static const char* VAR_FMT_VOLTAGE_L1 = "vltL1.val=%" PRIu16;
static const char* VAR_FMT_VOLTAGE_L2 = "vltL2.val=%" PRIu16;
static const char* VAR_FMT_VOLTAGE_L3 = "vltL3.val=%" PRIu16;
static const char* VAR_FMT_CURRENT_L1 = "curL1.val=%" PRIu16;
static const char* VAR_FMT_CURRENT_L2 = "curL2.val=%" PRIu16;
static const char* VAR_FMT_CURRENT_L3 = "curL3.val=%" PRIu16;
static const char* VAR_FMT_CONSUMPTION_LIMIT = "consumLim.val=%" PRIu32;
static const char* VAR_FMT_CHARGING_TIME_LIMIT = "chTimeLim.val=%" PRIu32;
static const char* VAR_FMT_UNDER_POWER_LIMIT = "uPowerLim.val=%" PRIu16;
static const char* VAR_FMT_DEFAULT_CONSUMPTION_LIMIT = "defConsumLim.val=%" PRIu32;
static const char* VAR_FMT_DEFAULT_CHARGING_TIME_LIMIT = "defChTimeLim.val=%" PRIu32;
static const char* VAR_FMT_DEFAULT_UNDER_POWER_LIMIT = "defUPowerLim.val=%" PRIu16;
static const char* VAR_FMT_DEVICE_NAME = "devName.txt=\"%s\"";
static const char* VAR_FMT_UPTIME = "uptime.val=%" PRIu32;
static const char* VAR_FMT_TEMPERATURE = "temp.val=%" PRIi16;  // TODO deprecated, same as VAR_FMT_HIGH_TEMPERATURE
static const char* VAR_FMT_LOW_TEMPERATURE = "loTemp.val=%" PRIi16;
static const char* VAR_FMT_HIGH_TEMPERATURE = "hiTemp.val=%" PRIi16;
static const char* VAR_FMT_IP = "ip.txt=\"%s\"";
static const char* VAR_FMT_APP_VERSION = "appVer.txt=\"%s\"";
static const char* VAR_FMT_HEAP_SIZE = "heap.val=%" PRIu32;
static const char* VAR_FMT_MAX_HEAP_SIZE = "maxHeap.val=%" PRIu32;
static const char* VAR_FMT_RTC = "rtc%d=%" PRIu32;

static const char* CMD_SUBSCRIBE = "sub %s";
static const char* CMD_UNSUBSCRIBE = "unsub %s";
static const char* CMD_UNSUBSCRIBE_ALL = "unsub";
static const char* CMD_ENABLED = "en %" PRIu8;
static const char* CMD_AVAILABLE = "avail %" PRIu8;
static const char* CMD_CHARGING_CURRENT = "chCur %" PRIu16;
static const char* CMD_CONSUMPTION_LIMIT = "consumLim %" PRIu32;
static const char* CMD_CHARGING_TIME_LIMIT = "chTimeLim %" PRIu32;
static const char* CMD_UNDER_POWER_LIMIT = "uPowerLim %" PRIu16;
static const char* CMD_AUTHORIZE = "auth";
static const char* CMD_REBOOT = "reboot";

static const char* NEX_CMD_WAKE = "sleep=0";
static const char* NEX_CMD_RESET = "rest";
static const char* NEX_CMD_FULL_BRIGHTNESS = "dims=100";

static const uint8_t DELIMITER[] = { 0xFF, 0xFF, 0xFF };

static const char* TAG = "serial_nextion";

static uart_port_t port = -1;

static uint32_t baud_rate;

static TaskHandle_t serial_nextion_task = NULL;

static SemaphoreHandle_t mutex = NULL;

static uint32_t upload_baud_rate;

static bool upload_first_chunk;

typedef struct {
    bool state : 1;
    bool enabled : 1;
    bool error : 1;
    bool pending_auth : 1;
    bool limit_reached : 1;
    bool charging_current : 1;
    bool max_charging_current : 1;
    bool default_charging_current : 1;
    bool session_time : 1;
    bool charging_time : 1;
    bool power : 1;
    bool consumption : 1;
    bool total_consumption : 1;
    bool voltage_l1 : 1;
    bool voltage_l2 : 1;
    bool voltage_l3 : 1;
    bool current_l1 : 1;
    bool current_l2 : 1;
    bool current_l3 : 1;
    bool consumption_limit : 1;
    bool charging_time_limit : 1;
    bool under_power_limit : 1;
    bool default_consumption_limit : 1;
    bool default_charging_time_limit : 1;
    bool default_under_power_limit : 1;
    bool uptime : 1;
    bool temperature : 1;  // TODO deprecated, same as high_temperature
    bool low_temperature : 1;
    bool high_temperature : 1;
    bool ip : 1;
    bool heap_size : 1;
    bool max_heap_size : 1;
} var_sub_t;

typedef struct {
    bool sleep : 1;
    var_sub_t var_sub;
    evse_state_t state;
    bool enabled;
    uint16_t charging_current;
    uint16_t max_charging_current;
    uint16_t default_charging_current;
    uint32_t consumption_limit;
    uint32_t charging_time_limit;
    uint16_t under_power_limit;
    uint32_t default_consumption_limit;
    uint32_t default_charging_time_limit;
    uint16_t default_under_power_limit;
    char ip[16];
} context_t;

static void tx_str(const char* cmd)
{
    uart_write_bytes(port, cmd, strlen(cmd));
    uart_write_bytes(port, DELIMITER, 3);
}

static void set_subscribe(context_t* ctx, const char* var, bool subscribe)
{
    char tx_cmd[64];

    if (!strcmp(var, VAR_STATE)) {
        ctx->var_sub.state = subscribe;
        ctx->state = EVSE_STATE_A;
    } else if (!strcmp(var, VAR_ENABLED)) {
        ctx->var_sub.enabled = subscribe;
        if (subscribe) {
            ctx->enabled = evse_is_enabled();
            sprintf(tx_cmd, VAR_FMT_ENABLED, ctx->enabled);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_ERROR)) {
        ctx->var_sub.error = subscribe;
    } else if (!strcmp(var, VAR_PENDING_AUTH)) {
        ctx->var_sub.pending_auth = subscribe;
    } else if (!strcmp(var, VAR_LIMIT_REACHED)) {
        ctx->var_sub.limit_reached = subscribe;
    } else if (!strcmp(var, VAR_CHARGING_CURRENT)) {
        ctx->var_sub.charging_current = subscribe;
        if (subscribe) {
            ctx->charging_current = evse_get_charging_current();
            sprintf(tx_cmd, VAR_FMT_CHARGING_CURRENT, ctx->charging_current);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_DEFAULT_CHARGING_CURRENT)) {
        ctx->var_sub.default_charging_current = subscribe;
        if (subscribe) {
            ctx->default_charging_current = evse_get_default_charging_current();
            sprintf(tx_cmd, VAR_FMT_DEFAULT_CHARGING_CURRENT, ctx->default_charging_current);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_MAX_CHARGING_CURRENT)) {
        ctx->var_sub.max_charging_current = subscribe;
        if (subscribe) {
            ctx->max_charging_current = evse_get_max_charging_current();
            sprintf(tx_cmd, VAR_FMT_MAX_CHARGING_CURRENT, ctx->max_charging_current);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_SESSION_TIME)) {
        ctx->var_sub.session_time = subscribe;
    } else if (!strcmp(var, VAR_CHARGING_TIME)) {
        ctx->var_sub.charging_time = subscribe;
    } else if (!strcmp(var, VAR_POWER)) {
        ctx->var_sub.power = subscribe;
    } else if (!strcmp(var, VAR_CONSUMPTION)) {
        ctx->var_sub.consumption = subscribe;
    } else if (!strcmp(var, VAR_TOTAL_CONSUMPTION)) {
        ctx->var_sub.total_consumption = subscribe;
    } else if (!strcmp(var, VAR_VOLTAGE_L1)) {
        ctx->var_sub.voltage_l1 = subscribe;
    } else if (!strcmp(var, VAR_VOLTAGE_L2)) {
        ctx->var_sub.voltage_l2 = subscribe;
    } else if (!strcmp(var, VAR_VOLTAGE_L3)) {
        ctx->var_sub.voltage_l3 = subscribe;
    } else if (!strcmp(var, VAR_CURRENT_L1)) {
        ctx->var_sub.current_l1 = subscribe;
    } else if (!strcmp(var, VAR_CURRENT_L2)) {
        ctx->var_sub.current_l2 = subscribe;
    } else if (!strcmp(var, VAR_CURRENT_L3)) {
        ctx->var_sub.current_l3 = subscribe;
    } else if (!strcmp(var, VAR_CONSUMPTION_LIMIT)) {
        ctx->var_sub.consumption_limit = subscribe;
        if (subscribe) {
            ctx->consumption_limit = evse_get_consumption_limit();
            sprintf(tx_cmd, VAR_FMT_CONSUMPTION_LIMIT, ctx->consumption_limit);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_CHARGING_TIME_LIMIT)) {
        ctx->var_sub.charging_time_limit = subscribe;
        ctx->charging_time_limit = evse_get_charging_time_limit();
        sprintf(tx_cmd, VAR_FMT_CHARGING_TIME_LIMIT, ctx->charging_time_limit);
        tx_str(tx_cmd);
    } else if (!strcmp(var, VAR_UNDER_POWER_LIMIT)) {
        ctx->var_sub.default_under_power_limit = subscribe;
        if (subscribe) {
            ctx->default_under_power_limit = evse_get_default_under_power_limit();
            sprintf(tx_cmd, VAR_FMT_UNDER_POWER_LIMIT, ctx->default_under_power_limit);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_DEFAULT_CONSUMPTION_LIMIT)) {
        ctx->var_sub.default_consumption_limit = subscribe;
        if (subscribe) {
            ctx->default_consumption_limit = evse_get_default_consumption_limit();
            sprintf(tx_cmd, VAR_FMT_DEFAULT_CONSUMPTION_LIMIT, ctx->default_consumption_limit);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_DEFAULT_CHARGING_TIME_LIMIT)) {
        ctx->var_sub.default_charging_time_limit = subscribe;
        ctx->default_charging_time_limit = evse_get_default_charging_time_limit();
        sprintf(tx_cmd, VAR_FMT_DEFAULT_CHARGING_TIME_LIMIT, ctx->default_charging_time_limit);
        tx_str(tx_cmd);
    } else if (!strcmp(var, VAR_DEFAULT_UNDER_POWER_LIMIT)) {
        ctx->var_sub.default_under_power_limit = subscribe;
        if (subscribe) {
            ctx->default_under_power_limit = evse_get_default_under_power_limit();
            sprintf(tx_cmd, VAR_FMT_DEFAULT_UNDER_POWER_LIMIT, ctx->default_under_power_limit);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_UPTIME)) {
        ctx->var_sub.uptime = subscribe;
    } else if (!strcmp(var, VAR_TEMPERATURE)) {
        ctx->var_sub.temperature = subscribe;
    } else if (!strcmp(var, VAR_LOW_TEMPERATURE)) {
        ctx->var_sub.low_temperature = subscribe;
    } else if (!strcmp(var, VAR_HIGH_TEMPERATURE)) {
        ctx->var_sub.high_temperature = subscribe;
    } else if (!strcmp(var, VAR_IP)) {
        ctx->var_sub.ip = subscribe;
        if (subscribe) {
            wifi_get_ip(false, ctx->ip);
            sprintf(tx_cmd, VAR_FMT_IP, ctx->ip);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_HEAP_SIZE)) {
        ctx->var_sub.heap_size = subscribe;
    } else if (!strcmp(var, VAR_MAX_HEAP_SIZE)) {
        ctx->var_sub.max_heap_size = subscribe;
    } else if (!strcmp(var, VAR_DEVICE_NAME)) {
        if (subscribe) {
            sprintf(tx_cmd, VAR_FMT_DEVICE_NAME, board_config.device_name);
            tx_str(tx_cmd);
        }
    } else if (!strcmp(var, VAR_APP_VERSION)) {
        if (subscribe) {
            const esp_app_desc_t* app_desc = esp_app_get_description();
            sprintf(tx_cmd, VAR_FMT_APP_VERSION, app_desc->version);
            tx_str(tx_cmd);
        }
    } else {
        ESP_LOGW(TAG, "Unknown variable: %s", var);
    }
}

static void handle_cmd(context_t* ctx, const uint8_t* cmd, uint8_t cmd_len)
{
    uint8_t var_u8;
    uint16_t var_u16;
    uint32_t var_u32;
    char var_str[32];
    char rx_cmd[64];

    // nextion return codes
    switch (cmd[0]) {
    case NEX_RET_BUF_OVERFLOW:
        ESP_LOGW(TAG, "Buffer overflow");
        vTaskDelay(pdMS_TO_TICKS(250));
        return;
    case NEX_RET_AUTO_SLEEP:
        ESP_LOGD(TAG, "Enter auto sleep");
        ctx->sleep = true;
        ctx->state = evse_get_state();
        return;
    case NEX_RET_READY:
        ESP_LOGI(TAG, "Display ready");
        break;
    default:
        break;
    }

    // commands
    strncpy(rx_cmd, (const char*)cmd, cmd_len);
    rx_cmd[cmd_len] = '\0';

    if (sscanf(rx_cmd, CMD_UNSUBSCRIBE, var_str) > 0) {
        ESP_LOGD(TAG, "Unsubscribe %s", var_str);
        set_subscribe(ctx, var_str, false);
    } else if (strcmp(rx_cmd, CMD_UNSUBSCRIBE_ALL) == 0) {
        ESP_LOGD(TAG, "Unsubscribe all");
        memset((void*)&ctx->var_sub, 0, sizeof(var_sub_t));
    } else if (sscanf(rx_cmd, CMD_SUBSCRIBE, var_str) > 0) {
        ESP_LOGD(TAG, "Subscribe %s", var_str);
        ctx->sleep = false;
        set_subscribe(ctx, var_str, true);
    } else if (sscanf(rx_cmd, CMD_ENABLED, &var_u8) > 0) {
        evse_set_enabled(var_u8);
    } else if (sscanf(rx_cmd, CMD_AVAILABLE, &var_u8) > 0) {
        evse_set_available(var_u8);
    } else if (sscanf(rx_cmd, CMD_CHARGING_CURRENT, &var_u16) > 0) {
        evse_set_charging_current(var_u16);
    } else if (sscanf(rx_cmd, CMD_CONSUMPTION_LIMIT, &var_u32) > 0) {
        evse_set_consumption_limit(var_u32);
    } else if (sscanf(rx_cmd, CMD_CHARGING_TIME_LIMIT, &var_u32) > 0) {
        evse_set_charging_time_limit(var_u32);
    } else if (sscanf(rx_cmd, CMD_UNDER_POWER_LIMIT, &var_u16) > 0) {
        evse_set_under_power_limit(var_u16);
    } else if (strcmp(rx_cmd, CMD_AUTHORIZE) == 0) {
        evse_authorize();
    } else if (strcmp(rx_cmd, CMD_REBOOT) == 0) {
        esp_restart();
    }
}

static void tx_vars(context_t* ctx)
{
    char tx_cmd[64];

    if (ctx->var_sub.state) {
        sprintf(tx_cmd, VAR_FMT_STATE, evse_state_to_str(evse_get_state()));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.enabled && ctx->enabled != evse_is_enabled()) {
        ctx->enabled = evse_is_enabled();
        sprintf(tx_cmd, VAR_FMT_ENABLED, ctx->enabled);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.error) {
        sprintf(tx_cmd, VAR_FMT_ERROR, evse_get_error());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.pending_auth) {
        sprintf(tx_cmd, VAR_FMT_PENDING_AUTH, evse_is_pending_auth());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.limit_reached) {
        sprintf(tx_cmd, VAR_FMT_LIMIT_REACHED, evse_is_limit_reached());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.charging_current && ctx->charging_current != evse_get_charging_current()) {
        ctx->charging_current = evse_get_charging_current();
        sprintf(tx_cmd, VAR_FMT_CHARGING_CURRENT, ctx->charging_current);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.max_charging_current && ctx->max_charging_current != evse_get_max_charging_current()) {
        ctx->max_charging_current = evse_get_max_charging_current();
        sprintf(tx_cmd, VAR_FMT_MAX_CHARGING_CURRENT, ctx->max_charging_current);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.default_charging_current && ctx->default_charging_current != evse_get_default_charging_current()) {
        ctx->default_charging_current = evse_get_default_charging_current();
        sprintf(tx_cmd, VAR_FMT_DEFAULT_CHARGING_CURRENT, ctx->default_charging_current);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.session_time) {
        sprintf(tx_cmd, VAR_FMT_SESSION_TIME, energy_meter_get_session_time());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.charging_time) {
        sprintf(tx_cmd, VAR_FMT_CHARGING_TIME, energy_meter_get_charging_time());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.power) {
        sprintf(tx_cmd, VAR_FMT_POWER, energy_meter_get_power());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.consumption) {
        sprintf(tx_cmd, VAR_FMT_CONSUMPTION, energy_meter_get_consumption());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.total_consumption) {
        sprintf(tx_cmd, VAR_FMT_TOTAL_CONSUMPTION, energy_meter_get_total_consumption());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.voltage_l1) {
        sprintf(tx_cmd, VAR_FMT_VOLTAGE_L1, (uint16_t)(energy_meter_get_l1_voltage() * 100));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.voltage_l2) {
        sprintf(tx_cmd, VAR_FMT_VOLTAGE_L2, (uint16_t)(energy_meter_get_l2_voltage() * 100));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.voltage_l3) {
        sprintf(tx_cmd, VAR_FMT_VOLTAGE_L3, (uint16_t)(energy_meter_get_l3_voltage() * 100));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.current_l1) {
        sprintf(tx_cmd, VAR_FMT_CURRENT_L1, (uint16_t)(energy_meter_get_l1_current() * 100));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.current_l2) {
        sprintf(tx_cmd, VAR_FMT_CURRENT_L2, (uint16_t)(energy_meter_get_l2_current() * 100));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.current_l3) {
        sprintf(tx_cmd, VAR_FMT_CURRENT_L3, (uint16_t)(energy_meter_get_l3_current() * 100));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.consumption_limit && ctx->consumption_limit != evse_get_consumption_limit()) {
        ctx->consumption_limit = evse_get_consumption_limit();
        sprintf(tx_cmd, VAR_FMT_CONSUMPTION_LIMIT, ctx->consumption_limit);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.charging_time_limit && ctx->charging_time_limit != evse_get_charging_time_limit()) {
        ctx->charging_time_limit = evse_get_charging_time_limit();
        sprintf(tx_cmd, VAR_FMT_CHARGING_TIME_LIMIT, ctx->charging_time_limit);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.under_power_limit && ctx->under_power_limit != evse_get_under_power_limit()) {
        ctx->under_power_limit = evse_get_under_power_limit();
        sprintf(tx_cmd, VAR_FMT_UNDER_POWER_LIMIT, ctx->under_power_limit);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.default_consumption_limit && ctx->default_consumption_limit != evse_get_default_consumption_limit()) {
        ctx->default_consumption_limit = evse_get_default_consumption_limit();
        sprintf(tx_cmd, VAR_FMT_CONSUMPTION_LIMIT, ctx->default_consumption_limit);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.default_charging_time_limit && ctx->default_charging_time_limit != evse_get_default_charging_time_limit()) {
        ctx->default_charging_time_limit = evse_get_default_charging_time_limit();
        sprintf(tx_cmd, VAR_FMT_CHARGING_TIME_LIMIT, ctx->default_charging_time_limit);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.default_under_power_limit && ctx->default_under_power_limit != evse_get_default_under_power_limit()) {
        ctx->default_under_power_limit = evse_get_default_under_power_limit();
        sprintf(tx_cmd, VAR_FMT_UNDER_POWER_LIMIT, ctx->default_under_power_limit);
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.uptime) {
        sprintf(tx_cmd, VAR_FMT_UPTIME, (uint32_t)(esp_timer_get_time() / 1000000));
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.temperature) {
        sprintf(tx_cmd, VAR_FMT_TEMPERATURE, temp_sensor_get_high());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.low_temperature) {
        sprintf(tx_cmd, VAR_FMT_LOW_TEMPERATURE, temp_sensor_get_low());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.high_temperature) {
        sprintf(tx_cmd, VAR_FMT_HIGH_TEMPERATURE, temp_sensor_get_high());
        tx_str(tx_cmd);
    }
    if (ctx->var_sub.ip) {
        char str[16];
        wifi_get_ip(false, str);
        if (strncmp(ctx->ip, str, 16) != 0) {
            strncpy(ctx->ip, str, 16);
            sprintf(tx_cmd, VAR_FMT_IP, str);
            tx_str(tx_cmd);
        }
    }
    if (ctx->var_sub.heap_size || ctx->var_sub.max_heap_size) {
        multi_heap_info_t heap_info;
        heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL);
        if (ctx->var_sub.heap_size) {
            sprintf(tx_cmd, VAR_FMT_HEAP_SIZE, heap_info.total_allocated_bytes);
            tx_str(tx_cmd);
        }
        if (ctx->var_sub.max_heap_size) {
            sprintf(tx_cmd, VAR_FMT_MAX_HEAP_SIZE, heap_info.total_free_bytes + heap_info.total_allocated_bytes);
            tx_str(tx_cmd);
        }
    }
}

static void serial_nextion_task_func(void* param)
{
    uint8_t buf[BUF_SIZE];
    context_t ctx = { 0 };

    tx_str(NEX_CMD_RESET);
    tx_str(NEX_CMD_WAKE);

    while (true) {
        xSemaphoreTake(mutex, portMAX_DELAY);

        int len = uart_read_bytes(port, buf, BUF_SIZE, pdMS_TO_TICKS(0));
        if (len > 0) {
            int start = 0;
            for (int i = 1; i < len - 2; i++) {
                if (buf[i] == 0xFF && buf[i + 1] == 0xFF && buf[i + 2] == 0xFF) {
                    if (i < len - 3 && buf[i + 3] == 0xFF) {
                        // not last delimiter sequence in buffer
                        continue;
                    }
                    handle_cmd(&ctx, &buf[start], i - start);
                    start = i + 3;
                }
            }
        }

        if (ctx.sleep) {
            evse_state_t state = evse_get_state();
            if (ctx.state != state) {
                tx_str(NEX_CMD_WAKE);
                ctx.state = state;
            }
        } else {
            tx_vars(&ctx);
        }

        xSemaphoreGive(mutex);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void serial_nextion_start(uart_port_t uart_num, uint32_t _baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485)
{
    ESP_LOGI(TAG, "Starting on uart %d", uart_num);

    baud_rate = _baud_rate;

    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bit,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };

    esp_err_t err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config() returned 0x%x", err);
        return;
    }

    err = uart_driver_install(uart_num, BUF_SIZE, 0, 0, NULL, 0);
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

    mutex = xSemaphoreCreateMutex();

    xTaskCreate(serial_nextion_task_func, "serial_nextion_task", 4 * 1024, NULL, 5, &serial_nextion_task);
}

void serial_nextion_stop(void)
{
    ESP_LOGI(TAG, "Stopping");

    if (serial_nextion_task) {
        vTaskDelete(serial_nextion_task);
        serial_nextion_task = NULL;

        vSemaphoreDelete(mutex);
        mutex = NULL;
    }

    if (port != -1) {
        uart_driver_delete(port);
        port = -1;
    }
}

void serial_nextion_set_time(void)
{
    if (port != -1) {
        time_t now = time(NULL);
        struct tm timeinfo = { 0 };
        localtime_r(&now, &timeinfo);
        char tx_cmd[16];
        sprintf(tx_cmd, VAR_FMT_RTC, 0, timeinfo.tm_year + 1900);
        tx_str(tx_cmd);
        sprintf(tx_cmd, VAR_FMT_RTC, 1, timeinfo.tm_mon + 1);
        tx_str(tx_cmd);
        sprintf(tx_cmd, VAR_FMT_RTC, 2, timeinfo.tm_mday);
        tx_str(tx_cmd);
        sprintf(tx_cmd, VAR_FMT_RTC, 3, timeinfo.tm_hour);
        tx_str(tx_cmd);
        sprintf(tx_cmd, VAR_FMT_RTC, 4, timeinfo.tm_min);
        tx_str(tx_cmd);
        sprintf(tx_cmd, VAR_FMT_RTC, 5, timeinfo.tm_sec);
        tx_str(tx_cmd);
    }
}

esp_err_t serial_nextion_get_info(serial_nextion_info_t* info)
{
    esp_err_t ret = ESP_ERR_NOT_FOUND;

    if (serial_nextion_task) {
        ret = ESP_ERR_TIMEOUT;
        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(500))) {
            tx_str("connect");

            char buf[BUF_SIZE];
            int len = uart_read_bytes(port, buf, BUF_SIZE, pdMS_TO_TICKS(500));
            if (len > 0) {
                buf[len] = '\0';
                ret = ESP_ERR_INVALID_STATE;

                int touch;
                char* saveptr;
                char* token = strtok_r(buf, ",", &saveptr);
                for (int i = 0; token; token = strtok_r(NULL, ",", &saveptr), i++) {
                    switch (i) {
                    case 0:
                        if (sscanf(token, "comok %d", &touch) != EOF) {
                            info->touch = touch > 0;
                            ret = ESP_OK;
                        }
                        break;
                    case 2:
                        strcpy(info->model, token);
                        break;
                    case 6:
                        sscanf(token, "%" PRIu32, &info->flash_size);
                        break;
                    default:
                        break;
                    }
                }
            }

            xSemaphoreGive(mutex);
        }
    }

    return ret;
}

static char upload_read_response(void)
{
    for (uint8_t i = 0; i < 50; i++) {  // 50 * 100 ms
        char res;
        int len = uart_read_bytes(port, &res, sizeof(char), pdMS_TO_TICKS(100));
        if (len > 0) {
            return res;
        }
    }

    return 0x00;
}

static size_t upload_read_skip_to(void)
{
    for (uint8_t i = 0; i < 50; i++) {  // 50 * 100 ms
        char buf[10];
        int len = uart_read_bytes(port, buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (len >= 4) {
            size_t res = 0;
            for (int i = 0; i < 4; ++i) {
                res += (buf[i] << (8 * i));
            }
            return res;
        }
    }

    return 0;
}

esp_err_t serial_nextion_upload_begin(size_t file_size, uint32_t _baud_rate)
{
    if (!serial_nextion_task) {
        return ESP_ERR_NOT_FOUND;
    }

    if (!xSemaphoreTake(mutex, pdMS_TO_TICKS(500))) {
        return ESP_ERR_TIMEOUT;
    }

    vTaskDelete(serial_nextion_task);  // task will be resumed in serial_nextion_upload_end
    serial_nextion_task = NULL;

    upload_baud_rate = _baud_rate;

    tx_str(NEX_CMD_FULL_BRIGHTNESS);
    tx_str(NEX_CMD_WAKE);
    vTaskDelay(pdMS_TO_TICKS(250));
    uart_flush_input(port);

    char buf[BUF_SIZE];
    sprintf(buf, "whmi-wris %" PRIu32 ",%" PRIu32 ",1", (uint32_t)file_size, upload_baud_rate);  // TODO why PRIuSIZE for size_t not work?
    tx_str(buf);

    if (upload_baud_rate != baud_rate) {
        uart_wait_tx_done(port, pdMS_TO_TICKS(1000));
        uart_set_baudrate(port, upload_baud_rate);
    }

    if (upload_read_response() != 0x05) {
        serial_nextion_upload_end();
        return ESP_ERR_INVALID_STATE;
    }

    upload_first_chunk = true;

    return ESP_OK;
}

esp_err_t serial_nextion_upload_write(char* data, uint16_t len, size_t* skip_to)
{
    uart_write_bytes(port, data, len);

    switch (upload_read_response()) {
    case 0x05:
        *skip_to = 0;
        break;
    case 0x08:
        *skip_to = upload_read_skip_to();
        break;
    default:
        serial_nextion_upload_end();
        return ESP_ERR_INVALID_STATE;
    }

    upload_first_chunk = false;

    return ESP_OK;
}

void serial_nextion_upload_end(void)
{
    if (upload_baud_rate != baud_rate) {
        uart_set_baudrate(port, baud_rate);
    }

    xSemaphoreGive(mutex);

    xTaskCreate(serial_nextion_task_func, "serial_nextion_task", 4 * 1024, NULL, 5, &serial_nextion_task);
}
