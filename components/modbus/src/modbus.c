#include "modbus.h"

#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <nvs.h>
#include <string.h>

#include "sdkconfig.h"

#include "energy_meter.h"
#include "evse.h"
#include "socket_lock.h"
#include "temp_sensor.h"

#define MODBUS_REG_STATE           100
#define MODBUS_REG_ERROR           101  // 2 word
#define MODBUS_REG_ENABLED         103
#define MODBUS_REG_AVAILABLE       104
#define MODBUS_REG_PENDING_AUTH    105
#define MODBUS_REG_CHR_CURRENT     106
#define MODBUS_REG_CONSUMPTION_LIM 107  // 2 word
#define MODBUS_REG_CHR_TIME_LIM    109  // 2 word
#define MODBUS_REG_UNDER_POWER_LIM 111
#define MODBUS_REG_AUTHORISE       112

#define MODBUS_REG_EMETER_POWER       200
#define MODBUS_REG_EMETER_SES_TIME    201  // 2 word
#define MODBUS_REG_EMETER_CHR_TIME    203  // 2 word
#define MODBUS_REG_EMETER_CONSUMPTION 205  // 2 word
#define MODBUS_REG_EMETER_L1_VTL      207  // 2 word
#define MODBUS_REG_EMETER_L2_VTL      209  // 2 word
#define MODBUS_REG_EMETER_L3_VTL      211  // 2 word
#define MODBUS_REG_EMETER_L1_CUR      213  // 2 word
#define MODBUS_REG_EMETER_L2_CUR      215  // 2 word
#define MODBUS_REG_EMETER_L3_CUR      217  // 2 word

#define MODBUS_REG_SOCKET_OUTLET       300
#define MODBUS_REG_RCM                 301
#define MODBUS_REG_TEMP_THRESHOLD      302
#define MODBUS_REG_REQ_AUTH            303
#define MODBUS_REG_MAX_CHR_CURRENT     304
#define MODBUS_REG_DEF_CHR_CURRENT     305
#define MODBUS_REG_DEF_CONSUMPTION_LIM 306  // 2 word
#define MODBUS_REG_DEF_CHR_TIME_LIM    308  // 2 word
#define MODBUS_REG_DEF_UNDER_POWER_LIM 310
#define MODBUS_REG_LOCK_OPERATING_TIME 311
#define MODBUS_REG_LOCK_BRAKE_TIME     312
#define MODBUS_REG_LOCK_DET_HI         313
#define MODBUS_REG_LOCK_RET_COUNT      314
#define MODBUS_REG_EMETER_MODE         315
#define MODBUS_REG_EMETER_AC_VLT       316
#define MODBUS_REG_EMETER_THREE_PHASES 317

#define MODBUS_REG_UPTIME            400  // 2 word
#define MODBUS_REG_TEMP_LOW          402
#define MODBUS_REG_TEMP_HIGH         403
#define MODBUS_REG_TEMP_SENSOR_COUNT 404
#define MODBUS_REG_APP_VERSION       405  // 16 word
#define MODBUS_REG_RESTART           421

#define MODBUS_EX_NONE                 0x00
#define MODBUS_EX_ILLEGAL_FUNCTION     0x01
#define MODBUS_EX_ILLEGAL_DATA_ADDRESS 0x02
#define MODBUS_EX_ILLEGAL_DATA_VALUE   0x03
#define MODBUS_EX_SLAVE_DEVICE_FAILURE 0x04
#define MODBUS_EX_ACKNOWLEDGE          0x05
#define MODBUS_EX_SLAVE_BUSY           0x06
#define MODBUS_EX_MEMORY_PARITY_ERROR  0x08

#define UINT32_GET_HI(value) ((uint16_t)(((uint32_t)(value)) >> 16))
#define UINT32_GET_LO(value) ((uint16_t)(((uint32_t)(value)) & 0xFFFF))

#define NVS_NAMESPACE "modbus"
#define NVS_UNIT_ID   "unit_id"

static const char* TAG = "modbus";

static nvs_handle nvs;

static uint8_t unit_id = 1;

static void restart_func(void* arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
    vTaskDelete(NULL);
}

static void timeout_restart()
{
    xTaskCreate(restart_func, "restart_task", 2 * 1024, NULL, 10, NULL);
}

void modbus_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    nvs_get_u8(nvs, NVS_UNIT_ID, &unit_id);
}

static uint32_t get_uptime(void)
{
    return esp_timer_get_time() / 1000000;
}

static bool read_holding_register(uint16_t addr, uint16_t* value)
{
    ESP_LOGD(TAG, "HR read %d", addr);
    switch (addr) {
    case MODBUS_REG_STATE:
        const char* state_str = evse_state_to_str(evse_get_state());
        *value = state_str[0] << 8 | state_str[1];
        break;
    case MODBUS_REG_ERROR:
        *value = UINT32_GET_HI(evse_get_error());
        break;
    case MODBUS_REG_ERROR + 1:
        *value = UINT32_GET_LO(evse_get_error());
        break;
    case MODBUS_REG_ENABLED:
        *value = evse_is_enabled();
        break;
    case MODBUS_REG_AVAILABLE:
        *value = evse_is_available();
        break;
    case MODBUS_REG_PENDING_AUTH:
        *value = evse_is_pending_auth();
        break;
    case MODBUS_REG_CHR_CURRENT:
        *value = evse_get_charging_current();
        break;
    case MODBUS_REG_CONSUMPTION_LIM:
        *value = UINT32_GET_HI(evse_get_consumption_limit());
        break;
    case MODBUS_REG_CONSUMPTION_LIM + 1:
        *value = UINT32_GET_LO(evse_get_consumption_limit());
        break;
    case MODBUS_REG_CHR_TIME_LIM:
        *value = UINT32_GET_HI(evse_get_charging_time_limit());
        break;
    case MODBUS_REG_CHR_TIME_LIM + 1:
        *value = UINT32_GET_LO(evse_get_charging_time_limit());
        break;
    case MODBUS_REG_UNDER_POWER_LIM:
        *value = evse_get_under_power_limit();
        break;
    case MODBUS_REG_EMETER_POWER:
        *value = energy_meter_get_power();
        break;
    case MODBUS_REG_EMETER_SES_TIME:
        *value = UINT32_GET_HI(energy_meter_get_session_time());
        break;
    case MODBUS_REG_EMETER_SES_TIME + 1:
        *value = UINT32_GET_LO(energy_meter_get_session_time());
        break;
    case MODBUS_REG_EMETER_CHR_TIME:
        *value = UINT32_GET_HI(energy_meter_get_charging_time());
        break;
    case MODBUS_REG_EMETER_CHR_TIME + 1:
        *value = UINT32_GET_LO(energy_meter_get_charging_time());
        break;
    case MODBUS_REG_EMETER_CONSUMPTION:
        *value = UINT32_GET_HI(energy_meter_get_consumption());
        break;
    case MODBUS_REG_EMETER_CONSUMPTION + 1:
        *value = UINT32_GET_LO(energy_meter_get_consumption());
        break;
    case MODBUS_REG_EMETER_L1_VTL:
        *value = UINT32_GET_HI(energy_meter_get_l1_voltage() * 1000);
        break;
    case MODBUS_REG_EMETER_L1_VTL + 1:
        *value = UINT32_GET_LO(energy_meter_get_l1_voltage() * 1000);
        break;
    case MODBUS_REG_EMETER_L2_VTL:
        *value = UINT32_GET_HI(energy_meter_get_l2_voltage() * 1000);
        break;
    case MODBUS_REG_EMETER_L2_VTL + 1:
        *value = UINT32_GET_LO(energy_meter_get_l2_voltage() * 1000);
        break;
    case MODBUS_REG_EMETER_L3_VTL:
        *value = UINT32_GET_HI(energy_meter_get_l3_voltage() * 1000);
        break;
    case MODBUS_REG_EMETER_L3_VTL + 1:
        *value = UINT32_GET_LO(energy_meter_get_l3_voltage() * 1000);
        break;
    case MODBUS_REG_EMETER_L1_CUR:
        *value = UINT32_GET_HI(energy_meter_get_l1_current() * 1000);
        break;
    case MODBUS_REG_EMETER_L1_CUR + 1:
        *value = UINT32_GET_LO(energy_meter_get_l1_current() * 1000);
        break;
    case MODBUS_REG_EMETER_L2_CUR:
        *value = UINT32_GET_HI(energy_meter_get_l2_current() * 1000);
        break;
    case MODBUS_REG_EMETER_L2_CUR + 1:
        *value = UINT32_GET_LO(energy_meter_get_l2_current() * 1000);
        break;
    case MODBUS_REG_EMETER_L3_CUR:
        *value = UINT32_GET_HI(energy_meter_get_l3_current() * 1000);
        break;
    case MODBUS_REG_EMETER_L3_CUR + 1:
        *value = UINT32_GET_LO(energy_meter_get_l3_current() * 1000);
        break;
    case MODBUS_REG_SOCKET_OUTLET:
        *value = evse_get_socket_outlet();
        break;
    case MODBUS_REG_RCM:
        *value = evse_is_rcm();
        break;
    case MODBUS_REG_TEMP_THRESHOLD:
        *value = evse_get_temp_threshold();
        break;
    case MODBUS_REG_REQ_AUTH:
        *value = evse_is_require_auth();
        break;
    case MODBUS_REG_MAX_CHR_CURRENT:
        *value = evse_get_max_charging_current();
        break;
    case MODBUS_REG_DEF_CHR_CURRENT:
        *value = evse_get_default_charging_current();
        break;
    case MODBUS_REG_DEF_CONSUMPTION_LIM:
        *value = UINT32_GET_HI(evse_get_default_consumption_limit());
        break;
    case MODBUS_REG_DEF_CONSUMPTION_LIM + 1:
        *value = UINT32_GET_LO(evse_get_default_consumption_limit());
        break;
    case MODBUS_REG_DEF_CHR_TIME_LIM:
        *value = UINT32_GET_HI(evse_get_default_charging_time_limit());
        break;
    case MODBUS_REG_DEF_CHR_TIME_LIM + 1:
        *value = UINT32_GET_LO(evse_get_default_charging_time_limit());
        break;
    case MODBUS_REG_DEF_UNDER_POWER_LIM:
        *value = evse_get_default_under_power_limit();
        break;
    case MODBUS_REG_LOCK_OPERATING_TIME:
        *value = socket_lock_get_operating_time();
        break;
    case MODBUS_REG_LOCK_BRAKE_TIME:
        *value = socket_lock_get_break_time();
        break;
    case MODBUS_REG_LOCK_DET_HI:
        *value = socket_lock_is_detection_high();
        break;
    case MODBUS_REG_LOCK_RET_COUNT:
        *value = socket_lock_get_retry_count();
        break;
    case MODBUS_REG_EMETER_MODE:
        *value = energy_meter_get_mode();
        break;
    case MODBUS_REG_EMETER_AC_VLT:
        *value = energy_meter_get_ac_voltage();
        break;
    case MODBUS_REG_EMETER_THREE_PHASES:
        *value = energy_meter_is_three_phases();
        break;
    case MODBUS_REG_UPTIME:
        *value = UINT32_GET_HI(get_uptime());
        break;
    case MODBUS_REG_UPTIME + 1:
        *value = UINT32_GET_LO(get_uptime());
        break;
    case MODBUS_REG_TEMP_LOW:
        *value = temp_sensor_get_low();
        break;
    case MODBUS_REG_TEMP_HIGH:
        *value = temp_sensor_get_high();
        break;
    case MODBUS_REG_TEMP_SENSOR_COUNT:
        *value = temp_sensor_get_count();
        break;
    default:
        // string registers
        if (addr >= MODBUS_REG_APP_VERSION && addr <= MODBUS_REG_APP_VERSION + 16) {
            const esp_app_desc_t* app_desc = esp_app_get_description();
            *value = app_desc->version[(addr - MODBUS_REG_APP_VERSION) * 2] << 8 | app_desc->version[(addr - MODBUS_REG_APP_VERSION) * 2 + 1];
        } else {
            return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
        }
    }
    return MODBUS_EX_NONE;
}

static bool write_holding_register(uint16_t addr, uint8_t* buffer, uint16_t left)
{
    uint16_t value = MODBUS_READ_UINT16(buffer, 0);
    ESP_LOGD(TAG, "HR write %d = %d", addr, value);
    switch (addr) {
    case MODBUS_REG_ENABLED:
        if (value > 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        evse_set_enabled(value);
        break;
    case MODBUS_REG_AVAILABLE:
        if (value > 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        evse_set_available(value);
        break;
    case MODBUS_REG_CHR_CURRENT:
        if (evse_set_charging_current(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_CONSUMPTION_LIM:
        if (left > 0) {
            evse_set_consumption_limit(value << 16 | MODBUS_READ_UINT16(buffer, 2));
        } else {
            return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
        }
        break;
    case MODBUS_REG_CONSUMPTION_LIM + 1:
        break;
    case MODBUS_REG_CHR_TIME_LIM:
        if (left > 0) {
            evse_set_charging_time_limit(value << 16 | MODBUS_READ_UINT16(buffer, 2));
        } else {
            return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
        }
        break;
    case MODBUS_REG_CHR_TIME_LIM + 1:
        break;
    case MODBUS_REG_UNDER_POWER_LIM:
        evse_set_under_power_limit(value);
        break;
    case MODBUS_REG_AUTHORISE:
        if (value != 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        evse_authorize();
        break;
    case MODBUS_REG_SOCKET_OUTLET:
        if (value != 0 || value != 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        if (evse_set_socket_outlet(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_RCM:
        if (value != 0 || value != 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        if (evse_set_rcm(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_TEMP_THRESHOLD:
        if (evse_set_temp_threshold(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_REQ_AUTH:
        if (value != 0 || value != 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        evse_set_require_auth(value);
        break;
    case MODBUS_REG_MAX_CHR_CURRENT:
        if (evse_set_max_charging_current(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_DEF_CHR_CURRENT:
        if (evse_set_default_charging_current(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_DEF_CONSUMPTION_LIM:
        if (left > 0) {
            evse_set_default_consumption_limit(value << 16 | MODBUS_READ_UINT16(buffer, 2));
        } else {
            return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
        }
        break;
    case MODBUS_REG_DEF_CONSUMPTION_LIM + 1:
        break;
    case MODBUS_REG_DEF_CHR_TIME_LIM:
        if (left > 0) {
            evse_set_default_charging_time_limit(value << 16 | MODBUS_READ_UINT16(buffer, 2));
        } else {
            return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
        }
        break;
    case MODBUS_REG_DEF_CHR_TIME_LIM + 1:
        break;
    case MODBUS_REG_DEF_UNDER_POWER_LIM:
        evse_set_default_under_power_limit(value);
        break;
    case MODBUS_REG_LOCK_OPERATING_TIME:
        if (socket_lock_set_operating_time(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_LOCK_BRAKE_TIME:
        if (socket_lock_set_break_time(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_LOCK_DET_HI:
        if (value != 0 || value != 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        socket_lock_set_detection_high(value);
        break;
    case MODBUS_REG_LOCK_RET_COUNT:
        if (value > UINT8_MAX) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        socket_lock_set_retry_count(value);
        break;
    case MODBUS_REG_EMETER_MODE:
        if (energy_meter_set_mode(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_EMETER_AC_VLT:
        if (energy_meter_set_ac_voltage(value) != ESP_OK) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        break;
    case MODBUS_REG_EMETER_THREE_PHASES:
        if (value != 0 || value != 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        energy_meter_set_three_phases(value);
        break;
    case MODBUS_REG_RESTART:
        if (value != 1) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        timeout_restart();
        break;
    default:
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }
    return MODBUS_EX_NONE;
}

uint16_t modbus_request_exec(uint8_t* data, uint16_t len)
{
    uint16_t resp_len = 0;

    if (unit_id == data[0]) {
        uint8_t fc = data[1];
        uint16_t addr;
        uint16_t count;
        uint16_t value;
        uint8_t ex = MODBUS_EX_NONE;

        if (fc == 3) {
            addr = MODBUS_READ_UINT16(data, 2);
            count = MODBUS_READ_UINT16(data, 4);

            data[2] = count * 2;
            resp_len = 3 + count * 2;

            for (uint16_t i = 0; i < count; i++) {
                if ((ex = read_holding_register(addr + i, &value)) != MODBUS_EX_NONE) {
                    break;
                }
                MODBUS_WRITE_UINT16(data, 3 + 2 * i, value);
            }
        } else if (fc == 6) {
            addr = MODBUS_READ_UINT16(data, 2);

            resp_len = 6;

            ex = write_holding_register(addr, &data[4], 0);
        } else if (fc == 16) {
            addr = MODBUS_READ_UINT16(data, 2);
            count = MODBUS_READ_UINT16(data, 4);

            resp_len = 6;

            for (uint16_t i = 0; i < count; i++) {
                if ((ex = write_holding_register(addr + i, &data[7 + 2 * i], count - i)) != MODBUS_EX_NONE) {
                    break;
                }
            }
        } else {
            ex = MODBUS_EX_ILLEGAL_FUNCTION;
        }

        if (ex != MODBUS_EX_NONE) {
            data[1] = 0x8 | fc;
            data[2] = ex;
            resp_len = 3;
        }
    }

    return resp_len;
}

uint8_t modbus_get_unit_id(void)
{
    return unit_id;
}

esp_err_t modbus_set_unit_id(uint8_t _unit_id)
{
    if (_unit_id == 0) {
        ESP_LOGE(TAG, "Unit id cant be 0");
        return ESP_ERR_INVALID_ARG;
    }

    unit_id = _unit_id;
    nvs_set_u8(nvs, NVS_UNIT_ID, unit_id);

    return ESP_OK;
}
