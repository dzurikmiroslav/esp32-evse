#include <string.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "nvs.h"

#include "modbus.h"
#include "wifi.h"
#include "evse.h"
#include "energy_meter.h"
#include "socket_lock.h"
#include "timeout_utils.h"

#define MODBUS_REG_STATE                100
#define MODBUS_REG_ERROR                101
#define MODBUS_REG_ENABLED              102
#define MODBUS_REG_PENDING_AUTH         103
#define MODBUS_REG_CHR_CURRENT          104
#define MODBUS_REG_CONSUMTION_LIM       105 // 2 word
#define MODBUS_REG_ELAPSED_LIM          107 // 2 word
#define MODBUS_REG_UNDER_POWER_LIM      109
#define MODBUS_REG_AUTHORISE            110

#define MODBUS_REG_EMETER_POWER         200
#define MODBUS_REG_EMETER_ELAPSED       201 // 2 word
#define MODBUS_REG_EMETER_CONSUMTION    203 // 2 word
#define MODBUS_REG_EMETER_L1_VTL        205 // 2 word
#define MODBUS_REG_EMETER_L2_VTL        207 // 2 word
#define MODBUS_REG_EMETER_L3_VTL        209 // 2 word
#define MODBUS_REG_EMETER_L1_CUR        211 // 2 word
#define MODBUS_REG_EMETER_L2_CUR        213 // 2 word
#define MODBUS_REG_EMETER_L3_CUR        215 // 2 word

#define MODBUS_REG_SOCKET_OUTLET        300
#define MODBUS_REG_RCM                  301
#define MODBUS_REG_REQ_AUTH             302
#define MODBUS_REG_DEF_CHR_CURRENT      303
#define MODBUS_REG_DEF_CONSUMTION_LIM   304 //2 word
#define MODBUS_REG_DEF_ELAPSED_LIM      306 //2 word
#define MODBUS_REG_DEF_UNDER_POWER_LIM  308 
#define MODBUS_REG_LOCK_OPERATING_TIME  309 
#define MODBUS_REG_LOCK_BRAKE_TIME      310
#define MODBUS_REG_LOCK_DET_HI          311
#define MODBUS_REG_LOCK_RET_COUNT       312
#define MODBUS_REG_EMETER_MODE          313
#define MODBUS_REG_EMETER_AC_VLT        314
#define MODBUS_REG_EMETER_PULSE_AMOUNT  315

#define MODBUS_REG_UPTIME               400 //2 word
#define MODBUS_REG_APP_VERSION          402 //16 word  
#define MODBUS_REG_RESTART              418

#define MODBUS_EX_NONE                  0x00
#define MODBUS_EX_ILLEGAL_FUNCTION      0x01
#define MODBUS_EX_ILLEGAL_DATA_ADDRESS  0x02
#define MODBUS_EX_ILLEGAL_DATA_VALUE    0x03
#define MODBUS_EX_SLAVE_DEVICE_FAILURE  0x04
#define MODBUS_EX_ACKNOWLEDGE           0x05
#define MODBUS_EX_SLAVE_BUSY            0x06
#define MODBUS_EX_MEMORY_PARITY_ERROR   0x08

#define UINT32_GET_HI(value)            ((uint16_t)(((uint32_t) (value)) >> 16))
#define UINT32_GET_LO(value)            ((uint16_t)(((uint32_t) (value)) & 0xFFFF))

#define NVS_NAMESPACE                   "modbus"
#define NVS_UNIT_ID                     "unit_id"

static const char* TAG = "modbus";

static nvs_handle nvs;

static uint8_t unit_id = 1;

void modbus_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    nvs_get_u8(nvs, NVS_UNIT_ID, &unit_id);
}

static uint16_t get_uptime(void)
{
    return esp_timer_get_time() / 1000000;
}

static bool read_holding_register(uint16_t addr, uint16_t* value)
{
    ESP_LOGD(TAG, "HR read %d", addr);
    switch (addr) {
    case MODBUS_REG_STATE:
        *value = evse_get_state();
        break;
    case MODBUS_REG_ERROR:
        *value = evse_get_error();
        break;
    case MODBUS_REG_ENABLED:
        *value = evse_is_enabled();
        break;
    case MODBUS_REG_PENDING_AUTH:
        *value = evse_is_pending_auth();
        break;
    case MODBUS_REG_CHR_CURRENT:
        *value = evse_get_chaging_current();
        break;
    case MODBUS_REG_CONSUMTION_LIM:
        *value = UINT32_GET_HI(evse_get_consumption_limit());
        break;
    case MODBUS_REG_CONSUMTION_LIM + 1:
        *value = UINT32_GET_LO(evse_get_consumption_limit());
        break;
    case MODBUS_REG_ELAPSED_LIM:
        *value = UINT32_GET_HI(evse_get_elapsed_limit());
        break;
    case MODBUS_REG_ELAPSED_LIM + 1:
        *value = UINT32_GET_LO(evse_get_elapsed_limit());
        break;
    case MODBUS_REG_UNDER_POWER_LIM:
        *value = evse_get_under_power_limit();
        break;
    case MODBUS_REG_EMETER_POWER:
        *value = energy_meter_get_power();
        break;
    case MODBUS_REG_EMETER_ELAPSED:
        *value = UINT32_GET_HI(energy_meter_get_session_elapsed());
        break;
    case MODBUS_REG_EMETER_ELAPSED + 1:
        *value = UINT32_GET_LO(energy_meter_get_session_elapsed());
        break;
    case MODBUS_REG_EMETER_CONSUMTION:
        *value = UINT32_GET_HI(energy_meter_get_session_consumption());
        break;
    case MODBUS_REG_EMETER_CONSUMTION + 1:
        *value = UINT32_GET_LO(energy_meter_get_session_consumption());
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
    case MODBUS_REG_REQ_AUTH:
        *value = evse_is_require_auth();
        break;
    case MODBUS_REG_DEF_CHR_CURRENT:
        *value = evse_get_default_chaging_current();
        break;
    case MODBUS_REG_DEF_CONSUMTION_LIM:
        *value = UINT32_GET_HI(evse_get_default_consumption_limit());
        break;
    case MODBUS_REG_DEF_CONSUMTION_LIM + 1:
        *value = UINT32_GET_LO(evse_get_default_consumption_limit());
        break;
    case MODBUS_REG_DEF_ELAPSED_LIM:
        *value = UINT32_GET_HI(evse_get_default_elapsed_limit());
        break;
    case MODBUS_REG_DEF_ELAPSED_LIM + 1:
        *value = UINT32_GET_LO(evse_get_default_elapsed_limit());
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
    case MODBUS_REG_EMETER_PULSE_AMOUNT:
        *value = energy_meter_get_pulse_amount();
        break;
    case MODBUS_REG_UPTIME:
        *value = UINT32_GET_HI(get_uptime());
        break;
    case MODBUS_REG_UPTIME + 1:
        *value = UINT32_GET_LO(get_uptime());
        break;
    default:
        //string registers
        if (addr >= MODBUS_REG_APP_VERSION && addr <= MODBUS_REG_APP_VERSION + 16) {
            const esp_app_desc_t* app_desc = esp_ota_get_app_description();
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
    case MODBUS_REG_CONSUMTION_LIM:
        if (left > 0) {
            evse_set_consumption_limit(value << 16 | MODBUS_READ_UINT16(buffer, 2));
        } else {
            return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
        }
        break;
    case MODBUS_REG_CONSUMTION_LIM + 1:
        break;
    case MODBUS_REG_ELAPSED_LIM:
        if (left > 0) {
            evse_set_elapsed_limit(value << 16 | MODBUS_READ_UINT16(buffer, 2));
        } else {
            return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
        }
        break;
    case MODBUS_REG_ELAPSED_LIM + 1:
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
