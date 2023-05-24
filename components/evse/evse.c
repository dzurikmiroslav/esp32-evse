#include <sys/param.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"

#include "evse.h"
#include "board_config.h"
#include "pilot.h"
#include "proximity.h"
#include "ac_relay.h"
#include "socket_lock.h"
#include "energy_meter.h"
#include "rcm.h"
#include "temp_sensor.h"

#define CHARGING_CURRENT_MIN            60
#define AUTHORIZED_TIME                 60000  // 60sec
#define ERROR_WAIT_TIME                 60000  // 60sec
#define UNDER_POWER_TIME                60000  // 60sec
#define TEMP_THRESHOLD_MIN              40
#define TEMP_THRESHOLD_MAX              80

#define NVS_NAMESPACE                   "evse"
#define NVS_DEFAULT_CHARGING_CURRENT    "def_chrg_curr"
#define NVS_SOCKET_OUTLET               "socket_outlet"
#define NVS_RCM                         "rcm"
#define NVS_TEMP_THRESHOLD              "temp_threshold"
#define NVS_REQUIRE_AUTH                "require_auth"
#define NVS_DEFAULT_CONSUMPTION_LIMIT   "def_cons_lim"
#define NVS_DEFAULT_CHARGING_TIME_LIMIT "def_ch_time_lim"
#define NVS_DEFAULT_UNDER_POWER_LIMIT   "def_un_pwr_lim"

#define LIMIT_CONSUMPTION_BIT           BIT0
#define LIMIT_CHARGING_TIME_BIT         BIT1
#define LIMIT_UNDER_POWER_BIT           BIT2

#define RCM_SELFTEST_BIT                BIT0
#define RCM_SELFTEST_OK_BIT             BIT1
#define RCM_SELFTEST_PERFORMED_BIT      BIT2

static const char* TAG = "evse";

static nvs_handle nvs;

static SemaphoreHandle_t mutex;

static evse_state_t state = EVSE_STATE_A;   // not hold error state

static uint32_t error = 0;

static bool error_cleared = false;

static uint16_t charging_current;

static uint32_t consumption_limit = 0;

static uint32_t charging_time_limit = 0;

static uint16_t under_power_limit = 0;

static uint8_t reached_limit = 0;

static bool socket_outlet = false;

static bool rcm = false;

static uint8_t rcm_selftest = 0;

static uint8_t temp_threshold = 60;

static bool enabled = true;

static bool require_auth = false;

static bool authorized = false;

static TickType_t auth_grant_to = 0;

static TickType_t error_wait_to = 0;

static TickType_t under_power_start_time = 0;

static uint8_t cable_max_current = 63;

enum pilot_state_e {
    PILOT_STATE_12V,
    PILOT_STATE_N12V,
    PILOT_STATE_PWM
};

static enum pilot_state_e pilot_state = PILOT_STATE_12V;

static bool socket_lock_locked = false;

static evse_state_t prev_state = EVSE_STATE_A;

static void set_error_bits(uint32_t bits)
{
    error |= bits;
    if (bits & EVSE_ERR_AUTO_CLEAR_BITS) {
        error_wait_to = xTaskGetTickCount() + pdMS_TO_TICKS(ERROR_WAIT_TIME);
    }
}

static void clear_error_bits(uint32_t bits)
{
    bool has_error = error != 0;
    error &= ~bits;

    error_cleared |= has_error && error == 0;
}

static void set_pilot(enum pilot_state_e state)
{
    if (pilot_state != state) {
        pilot_state = state;
        switch (pilot_state)
        {
        case PILOT_STATE_12V:
            pilot_set_level(true);
            break;
        case PILOT_STATE_N12V:
            pilot_set_level(false);
            break;
        case PILOT_STATE_PWM:
            pilot_set_amps(MIN(charging_current, cable_max_current * 10));
            break;
        }
    }
}

static void set_socket_lock(bool locked)
{
    if (error & (EVSE_ERR_LOCK_FAULT_BIT | EVSE_ERR_UNLOCK_FAULT_BIT)) {
        return;
    }
    if (socket_lock_locked != locked) {
        socket_lock_locked = locked;
        socket_lock_set_locked(socket_lock_locked);
    }
}

static void rcm_perform_selftest(void)
{
    if (!(rcm_selftest & RCM_SELFTEST_PERFORMED_BIT)) {
        ESP_LOGI(TAG, "Performing residual current monitor self test");

        rcm_selftest |= RCM_SELFTEST_BIT;
        rcm_selftest &= ~RCM_SELFTEST_OK_BIT;

        rcm_test();

        rcm_selftest &= ~RCM_SELFTEST_BIT;
        if (!(rcm_selftest & RCM_SELFTEST_OK_BIT)) {
            ESP_LOGW(TAG, "Residual current monitor self test fail");
            set_error_bits(EVSE_ERR_RCM_SELFTEST_FAULT_BIT);
        }

        rcm_selftest |= RCM_SELFTEST_PERFORMED_BIT;
    }
}

static void apply_state(void)
{
    evse_state_t new_state = evse_get_state(); // getter method detect error state

    if (prev_state != new_state) {
        ESP_LOGI(TAG, "Enter %c state", 'A' + new_state);
        if (error) {
            ESP_LOGI(TAG, "Error bits %"PRIu32"", error);
        }

        switch (new_state)
        {
        case EVSE_STATE_A:
        case EVSE_STATE_E:
        case EVSE_STATE_F:
            ac_relay_set_state(false);
            if (board_config.socket_lock && socket_outlet) {
                set_socket_lock(false);
            }
            set_pilot(new_state == EVSE_STATE_A ? PILOT_STATE_12V : PILOT_STATE_N12V);

            authorized = false;
            reached_limit = 0;
            under_power_start_time = 0;
            rcm_selftest = 0;
            energy_meter_stop_session();
            break;
        case EVSE_STATE_B:
            if (board_config.rcm && rcm) {
                rcm_perform_selftest();
            }        
            if (socket_outlet) {
                cable_max_current = proximity_get_max_current();
                if (board_config.socket_lock) {
                    set_socket_lock(true);
                }
            }
            ac_relay_set_state(false);
            energy_meter_start_session();
            break;
        case EVSE_STATE_C:
        case EVSE_STATE_D:
            ac_relay_set_state(true);
            break;
        }

        prev_state = new_state;
    }
}

static bool can_goto_state_c(void)
{
    if (!enabled) {
        return false;
    }

    if (!authorized) {
        return false;
    }

    if (reached_limit != 0) {
        return false;
    }

    if (socket_outlet && board_config.socket_lock && socket_lock_get_status() != SOCKED_LOCK_STATUS_IDLE) {
        return false;
    }

    return true;
}

void evse_process(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    pilot_voltage_t pilot_voltage;
    bool pilot_down_voltage_n12;
    pilot_measure(&pilot_voltage, &pilot_down_voltage_n12);

    if (error_wait_to != 0 && xTaskGetTickCount() >= error_wait_to) {
        clear_error_bits(EVSE_ERR_AUTO_CLEAR_BITS);
        state = EVSE_STATE_A;
        error_wait_to = 0;
    }

    if (pilot_state == PILOT_STATE_PWM && !pilot_down_voltage_n12) {
        set_error_bits(EVSE_ERR_DIODE_SHORT_BIT);
    }

    if (board_config.socket_lock && socket_outlet) {
        switch (socket_lock_get_status())
        {
        case SOCKED_LOCK_STATUS_LOCKING_FAIL:
            set_error_bits(EVSE_ERR_LOCK_FAULT_BIT);
            break;
        case SOCKED_LOCK_STATUS_UNLOCKING_FAIL:
            set_error_bits(EVSE_ERR_UNLOCK_FAULT_BIT);
            break;
        default:
            break;
        }
    }

    if (board_config.onewire && board_config.onewire_temp_sensor) {
        if (temp_sensor_get_high() > temp_threshold * 100) {
            set_error_bits(EVSE_ERR_TEMPERATURE_HIGH_BIT);
        } else {
            clear_error_bits(EVSE_ERR_TEMPERATURE_HIGH_BIT);
        }

        if (temp_sensor_is_error()) {
            set_error_bits(EVSE_ERR_TEMPERATURE_FAULT_BIT);
        } else {
            clear_error_bits(EVSE_ERR_TEMPERATURE_FAULT_BIT);
        }
    }

    if (error == 0 && !error_cleared) {
        //no errors
        //after clear error, process on next iteration, after apply_state

        switch (state)
        {
        case EVSE_STATE_A:
            switch (pilot_voltage)
            {
            case PILOT_VOLTAGE_12:
                // stay in current state
                break;
            case PILOT_VOLTAGE_9:
                state = EVSE_STATE_B;
                break;
            default:
                set_error_bits(EVSE_ERR_PILOT_FAULT_BIT);
            }
            break;
        case EVSE_STATE_B:
            if (!authorized) {
                if (require_auth) {
                    authorized = auth_grant_to >= xTaskGetTickCount();
                    auth_grant_to = 0;
                } else {
                    authorized = true;
                }
            }
            if (!enabled || !authorized || reached_limit > 0) {
                set_pilot(PILOT_STATE_12V);
            } else {
                set_pilot(PILOT_STATE_PWM);
            }
            switch (pilot_voltage)
            {
            case PILOT_VOLTAGE_12:
                state = EVSE_STATE_A;
                break;
            case PILOT_VOLTAGE_9:
                // stay in current state
                break;
            case PILOT_VOLTAGE_6:
                if (can_goto_state_c()) {
                    state = EVSE_STATE_C;
                }
                break;
            default:
                set_error_bits(EVSE_ERR_PILOT_FAULT_BIT);
            }
            break;
        case EVSE_STATE_C:
            switch (pilot_voltage)
            {
            case PILOT_VOLTAGE_12:
                state = EVSE_STATE_A;
                break;
            case PILOT_VOLTAGE_9:
                state = EVSE_STATE_B;
                break;
            case PILOT_VOLTAGE_6:
                // stay in current state
                break;
            case PILOT_VOLTAGE_3:
                state = EVSE_STATE_D;
                break;
            default:
                set_error_bits(EVSE_ERR_PILOT_FAULT_BIT);
            }
            break;
        case EVSE_STATE_D:
            switch (pilot_voltage)
            {
            case PILOT_VOLTAGE_6:
                state = EVSE_STATE_C;
                break;
            case PILOT_VOLTAGE_3:
                // stay in current state
                break;
            default:
                set_error_bits(EVSE_ERR_PILOT_FAULT_BIT);
            }
            break;
        case EVSE_STATE_E:
        case EVSE_STATE_F:
            break;
        }

        // check consumption limit
        if (consumption_limit > 0 && energy_meter_get_consumption() > consumption_limit) {
            reached_limit |= LIMIT_CONSUMPTION_BIT;
        } else {
            reached_limit &= ~LIMIT_CONSUMPTION_BIT;
        }

        // check charging time limit
        if (charging_time_limit > 0 && energy_meter_get_charging_time() > charging_time_limit) {
            reached_limit |= LIMIT_CHARGING_TIME_BIT;
        } else {
            reached_limit &= ~LIMIT_CHARGING_TIME_BIT;
        }

        // check under power limit
        if (evse_state_is_charging(state)) {
            if (under_power_limit > 0 && energy_meter_get_power() < under_power_limit) {
                if (under_power_start_time == 0) {
                    under_power_start_time = xTaskGetTickCount();
                }
            } else {
                under_power_start_time = 0;
            }

            if (under_power_start_time > 0 && under_power_start_time + pdMS_TO_TICKS(UNDER_POWER_TIME) < xTaskGetTickCount()) {
                reached_limit |= LIMIT_UNDER_POWER_BIT;
            } else {
                reached_limit &= ~LIMIT_UNDER_POWER_BIT;
            }
        }

        if (reached_limit > 0 && evse_state_is_charging(state)) {
            ESP_LOGI(TAG, "Reached limit %d", reached_limit);
            state = EVSE_STATE_B;
        }
    }

    apply_state();

    error_cleared = false;

    xSemaphoreGive(mutex);

    energy_meter_process(evse_state_is_charging(evse_get_state()), charging_current);
}

void evse_set_available(bool available)
{
    ESP_LOGI(TAG, "Set available %d", available);
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (available) {
        state = EVSE_STATE_A;
    } else {
        state = EVSE_STATE_F;
    }
    apply_state();

    xSemaphoreGive(mutex);
}

void evse_rcm_triggered_isr(void)
{
    if (rcm_selftest & RCM_SELFTEST_BIT) {
        rcm_selftest |= RCM_SELFTEST_OK_BIT;
    } else {
        error |= EVSE_ERR_RCM_TRIGGERED_BIT;
        error_wait_to = xTaskGetTickCount() + pdMS_TO_TICKS(ERROR_WAIT_TIME);
        ac_relay_set_state_isr(false);
    }
}

void evse_init()
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    mutex = xSemaphoreCreateMutex();

    charging_current = board_config.max_charging_current * 10;
    nvs_get_u16(nvs, NVS_DEFAULT_CHARGING_CURRENT, &charging_current);

    uint8_t u8;
    if (nvs_get_u8(nvs, NVS_REQUIRE_AUTH, &u8) == ESP_OK) {
        require_auth = u8;
    }

    if (nvs_get_u8(nvs, NVS_SOCKET_OUTLET, &u8) == ESP_OK) {
        socket_outlet = u8;
    }

    if (nvs_get_u8(nvs, NVS_RCM, &u8) == ESP_OK) {
        rcm = u8;
    }

    nvs_get_u8(nvs, NVS_TEMP_THRESHOLD, &temp_threshold);

    nvs_get_u32(nvs, NVS_DEFAULT_CONSUMPTION_LIMIT, &consumption_limit);

    nvs_get_u32(nvs, NVS_DEFAULT_CHARGING_TIME_LIMIT, &charging_time_limit);

    nvs_get_u16(nvs, NVS_DEFAULT_UNDER_POWER_LIMIT, &under_power_limit);

    pilot_set_level(true);
}

evse_state_t evse_get_state(void)
{
    return error ? EVSE_STATE_E : state;
}

uint32_t evse_get_error(void)
{
    return error;
}

uint16_t evse_get_charging_current(void)
{
    return charging_current;
}

esp_err_t evse_set_charging_current(uint16_t value)
{
    ESP_LOGI(TAG, "Set charging current %dA*10", value);

    if (value < CHARGING_CURRENT_MIN || value > board_config.max_charging_current * 10) {
        ESP_LOGE(TAG, "Charging current out of range");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);

    charging_current = value;

    if (pilot_state == PILOT_STATE_PWM) {
        pilot_set_amps(MIN(charging_current, cable_max_current * 10));
    }

    xSemaphoreGive(mutex);

    return ESP_OK;
}

uint16_t evse_get_default_charging_current(void)
{
    uint16_t value = board_config.max_charging_current * 10;
    nvs_get_u16(nvs, NVS_DEFAULT_CHARGING_CURRENT, &value);
    return value;
}

esp_err_t evse_set_default_charging_current(uint16_t value)
{
    ESP_LOGI(TAG, "Set default charging current %dA*10", value);

    if (value < CHARGING_CURRENT_MIN || value > board_config.max_charging_current * 10) {
        ESP_LOGE(TAG, "Default charging current out of range");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_set_u16(nvs, NVS_DEFAULT_CHARGING_CURRENT, value);
    nvs_commit(nvs);

    return ESP_OK;
}

bool evse_get_socket_outlet(void)
{
    return socket_outlet;
}

esp_err_t evse_set_socket_outlet(bool _socket_outlet)
{
    ESP_LOGI(TAG, "Set socket outlet %d", _socket_outlet);

    if (_socket_outlet && !(board_config.proximity)) {
        ESP_LOGE(TAG, "Cant work in socket outlet mode, proximity pilot not available");
        return ESP_ERR_INVALID_ARG;
    }

    socket_outlet = _socket_outlet;

    nvs_set_u8(nvs, NVS_SOCKET_OUTLET, socket_outlet);
    nvs_commit(nvs);

    return ESP_OK;
}

esp_err_t evse_set_rcm(bool _rcm)
{
    ESP_LOGI(TAG, "Set rcm %d", _rcm);

    if (_rcm && !board_config.rcm) {
        ESP_LOGE(TAG, "Residual current monitor not available");
        return ESP_ERR_INVALID_ARG;
    }

    rcm = _rcm;

    nvs_set_u8(nvs, NVS_RCM, rcm);
    nvs_commit(nvs);

    return ESP_OK;
}

bool evse_is_rcm(void)
{
    return rcm;
}

esp_err_t evse_set_temp_threshold(uint8_t _temp_threshold)
{
    ESP_LOGI(TAG, "Set temperature threshold %ddg.C", _temp_threshold);

    if (_temp_threshold < TEMP_THRESHOLD_MIN || _temp_threshold > TEMP_THRESHOLD_MAX) {
        ESP_LOGE(TAG, "Temperature threshold out of range");
        return ESP_ERR_INVALID_ARG;
    }

    temp_threshold = _temp_threshold;

    nvs_set_u8(nvs, NVS_TEMP_THRESHOLD, temp_threshold);
    nvs_commit(nvs);

    return ESP_OK;
}

uint8_t evse_get_temp_threshold(void)
{
    return temp_threshold;
}

bool evse_is_require_auth(void)
{
    return require_auth;
}

void evse_set_require_auth(bool _require_auth)
{
    ESP_LOGI(TAG, "Set require auth %d", _require_auth);

    require_auth = _require_auth;

    nvs_set_u8(nvs, NVS_REQUIRE_AUTH, require_auth);
    nvs_commit(nvs);
}

void evse_authorize(void)
{
    ESP_LOGI(TAG, "Authorize");
    auth_grant_to = xTaskGetTickCount() + pdMS_TO_TICKS(AUTHORIZED_TIME);
    under_power_start_time = 0;
}

bool evse_is_pending_auth(void)
{
    return evse_state_is_session(state) && !authorized;
}

bool evse_is_enabled(void)
{
    return enabled;
}

void evse_set_enabled(bool _enabled)
{
    ESP_LOGI(TAG, "Set enabled %d", _enabled);

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (enabled != _enabled) {
        enabled = _enabled;
        if (enabled) {
            under_power_start_time = 0;
        } else {
            if (evse_state_is_charging(state)) {
                state = EVSE_STATE_B;
            }
        }

        apply_state();
    }

    xSemaphoreGive(mutex);
}

bool evse_is_limit_reached(void)
{
    return reached_limit != 0;
}

uint32_t evse_get_consumption_limit(void)
{
    return consumption_limit;
}

void evse_set_consumption_limit(uint32_t value)
{
    consumption_limit = value;
}

uint32_t evse_get_charging_time_limit(void)
{
    return charging_time_limit;
}

void evse_set_charging_time_limit(uint32_t value)
{
    charging_time_limit = value;
}

uint16_t evse_get_under_power_limit(void)
{
    return under_power_limit;
}

void evse_set_under_power_limit(uint16_t value)
{
    under_power_limit = value;
}

uint32_t evse_get_default_consumption_limit(void)
{
    uint32_t value = 0;
    nvs_get_u32(nvs, NVS_DEFAULT_CONSUMPTION_LIMIT, &value);
    return value;
}

void evse_set_default_consumption_limit(uint32_t value)
{
    nvs_set_u32(nvs, NVS_DEFAULT_CONSUMPTION_LIMIT, value);
    nvs_commit(nvs);
}

uint32_t evse_get_default_charging_time_limit(void)
{
    uint32_t value = 0;
    nvs_get_u32(nvs, NVS_DEFAULT_CHARGING_TIME_LIMIT, &value);
    return value;
}

void evse_set_default_charging_time_limit(uint32_t value)
{
    nvs_set_u32(nvs, NVS_DEFAULT_CHARGING_TIME_LIMIT, value);
    nvs_commit(nvs);
}

uint16_t evse_get_default_under_power_limit(void)
{
    uint16_t value = 0;
    nvs_get_u16(nvs, NVS_DEFAULT_UNDER_POWER_LIMIT, &value);
    return value;
}

void evse_set_default_under_power_limit(uint16_t value)
{
    nvs_set_u16(nvs, NVS_DEFAULT_UNDER_POWER_LIMIT, value);
    nvs_commit(nvs);
}