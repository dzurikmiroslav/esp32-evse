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

#define CHARGING_CURRENT_MIN            60
#define AUTHORIZED_TIME                 60000  // 60sec
#define ERROR_WAIT_TIME                 60000  // 60sec
#define UNDER_POWER_TIME                60000  // 60sec

#define NVS_NAMESPACE                   "evse"
#define NVS_DEFAULT_CHARGING_CURRENT    "def_chrg_curr"
#define NVS_SOCKET_OUTLET               "socket_outlet"
#define NVS_RCM                         "rcm"
#define NVS_REQUIRE_AUTH                "require_auth"
#define NVS_DEFAULT_CONSUMTION_LIMIT    "def_cons_lim"
#define NVS_DEFAULT_ELAPSED_LIMIT       "def_elap_lim"
#define NVS_DEFAULT_UNDER_POWER_LIMIT   "def_un_pwr_lim"

#define LIMIT_CONSUMPTION_BIT           (1 << 0)
#define LIMIT_ELAPSED_BIT               (1 << 1)
#define LIMIT_UNDER_POWER_BIT           (1 << 2)

static const char* TAG = "evse";

static nvs_handle nvs;

static SemaphoreHandle_t mutex;

static evse_state_t state = EVSE_STATE_A;

static evse_err_t error = EVSE_ERR_NONE;

static uint16_t charging_current;

static uint32_t consumption_limit = 0;

static uint32_t elapsed_limit = 0;

static uint16_t under_power_limit = 0;

static uint8_t reached_limit = 0;

static bool socket_outlet = false;

static bool rcm = false;

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

static void set_pilot(enum pilot_state_e state)
{
    if (state != pilot_state) {
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
    if (socket_lock_locked != locked || error == EVSE_ERR_LOCK_FAULT || error == EVSE_ERR_UNLOCK_FAULT) {
        socket_lock_locked = locked;
        socket_lock_set_locked(socket_lock_locked);
    }
}

static void set_state(evse_state_t next_state)
{
    switch (next_state)
    {
    case EVSE_STATE_A:
        if ( board_config.socket_lock && socket_outlet) {
            set_socket_lock(false);
        }
        set_pilot(PILOT_STATE_12V);
        ac_relay_set_state(false);
        break;
    case EVSE_STATE_B:
        if (socket_outlet) {
            cable_max_current = proximity_get_max_current();
            if (board_config.socket_lock) {
                set_socket_lock(true);
            }
        }
        ac_relay_set_state(false);
        break;
    case EVSE_STATE_C:
    case EVSE_STATE_D:
        ac_relay_set_state(true);
        break;
    case EVSE_STATE_E:
    case EVSE_STATE_F:
        ac_relay_set_state(false);
        if ( board_config.socket_lock && socket_outlet) {
            set_socket_lock(false);
        }
        if (next_state == EVSE_STATE_E) {
            if (error == EVSE_ERR_RCM_SELFTEST_FAULT) {
                error_wait_to = 0;
            } else {
                error_wait_to = xTaskGetTickCount() + pdMS_TO_TICKS(ERROR_WAIT_TIME);
            }
        }
        pilot_set_level(false);
        set_pilot(PILOT_STATE_N12V);
        break;
    }

    ESP_LOGI(TAG, "Enter %c state", 'A' + next_state);
    if (next_state == EVSE_STATE_E) {
        ESP_LOGE(TAG, "Error number %d", error);
    }
    state = next_state;
}

static void state_update(void)
{
    switch (state)
    {
    case EVSE_STATE_A:
        authorized = false;
        reached_limit = 0;
        under_power_start_time = 0;
        break;
    case EVSE_STATE_B:
        if (!authorized && require_auth) {
            authorized = auth_grant_to >= xTaskGetTickCount();
            auth_grant_to = 0;
        }
        if (!enabled || (require_auth && !authorized) || reached_limit > 0) {
            set_pilot(PILOT_STATE_12V);
        } else {
            set_pilot(PILOT_STATE_PWM);
        }
        break;
    case EVSE_STATE_C:
    case EVSE_STATE_D:
        break;
    case EVSE_STATE_E:
    case EVSE_STATE_F:
        authorized = false;
        reached_limit = 0;
        break;
    }
}

static bool can_goto_state_c(void)
{
    if (!enabled) {
        return false;
    }

    if (require_auth && !authorized) {
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

    pilot_measure();
    evse_state_t next_state = state;

    if (state == EVSE_STATE_E && xTaskGetTickCount() > error_wait_to) {
        next_state = EVSE_STATE_A;
        error = EVSE_ERR_NONE;
    // TODO hotfix :)
    } else if (pilot_state == PILOT_STATE_PWM && !pilot_is_down_voltage_n12()) {
        next_state = EVSE_STATE_E;
        error = EVSE_ERR_DIODE_SHORT;
    } else if (socket_outlet && board_config.socket_lock && socket_lock_get_status() == SOCKED_LOCK_STATUS_LOCKING_FAIL && error != EVSE_ERR_LOCK_FAULT) {
        next_state = EVSE_STATE_E;
        error = EVSE_ERR_LOCK_FAULT;
    } else if (socket_outlet && board_config.socket_lock && socket_lock_get_status() == SOCKED_LOCK_STATUS_UNLOCKING_FAIL && error != EVSE_ERR_UNLOCK_FAULT) {
        next_state = EVSE_STATE_E;
        error = EVSE_ERR_UNLOCK_FAULT;
    } else if (rcm && rcm_was_triggered()) {
        next_state = EVSE_STATE_E;
        error = EVSE_ERR_RCM_TRIGGERED;
    } else {
        switch (state)
        {
        case EVSE_STATE_A:
            switch (pilot_get_up_voltage())
            {
            case PILOT_VOLTAGE_12:
                // stay in current state
                break;
            case PILOT_VOLTAGE_9:
                next_state = EVSE_STATE_B;
                break;
            default:
                next_state = EVSE_STATE_E;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_B:
            switch (pilot_get_up_voltage())
            {
            case PILOT_VOLTAGE_12:
                next_state = EVSE_STATE_A;
                break;
            case PILOT_VOLTAGE_9:
                // stay in current state
                break;
            case PILOT_VOLTAGE_6:
                if (can_goto_state_c()) {
                    next_state = EVSE_STATE_C;
                }
                break;
            default:
                next_state = EVSE_STATE_E;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_C:
            switch (pilot_get_up_voltage())
            {
            case PILOT_VOLTAGE_12:
                next_state = EVSE_STATE_A;
                break;
            case PILOT_VOLTAGE_9:
                next_state = EVSE_STATE_B;
                break;
            case PILOT_VOLTAGE_6:
                // stay in current state
                break;
            case PILOT_VOLTAGE_3:
                next_state = EVSE_STATE_D;
                break;
            default:
                next_state = EVSE_STATE_E;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_D:
            switch (pilot_get_up_voltage())
            {
            case PILOT_VOLTAGE_6:
                next_state = EVSE_STATE_C;
                break;
            case PILOT_VOLTAGE_3:
                // stay in current state
                break;
            default:
                next_state = EVSE_STATE_E;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_E:
        case EVSE_STATE_F:
            break;
        }
    }

    // check consumption limit
    if (consumption_limit > 0 && energy_meter_get_session_consumption() > consumption_limit) {
        reached_limit |= LIMIT_CONSUMPTION_BIT;
    } else {
        reached_limit &= ~LIMIT_CONSUMPTION_BIT;
    }

    // check elapsed limit
    if (elapsed_limit > 0 && energy_meter_get_session_elapsed() > elapsed_limit) {
        reached_limit |= LIMIT_ELAPSED_BIT;
    } else {
        reached_limit &= ~LIMIT_ELAPSED_BIT;
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
        ESP_LOGI(TAG, "Reached linit %d", reached_limit);
        next_state = EVSE_STATE_B;
    }

    if (next_state != state) {
        set_state(next_state);
    }

    state_update();

    xSemaphoreGive(mutex);

    energy_meter_process();
}

void evse_set_avalable(bool avalable)
{
    ESP_LOGI(TAG, "Set avalable %d", avalable);
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (avalable) {
        set_state(error != EVSE_ERR_NONE ? EVSE_STATE_A : EVSE_STATE_E);
    } else {
        set_state(EVSE_STATE_F);
    }

    xSemaphoreGive(mutex);
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

    nvs_get_u32(nvs, NVS_DEFAULT_CONSUMTION_LIMIT, &consumption_limit);

    nvs_get_u32(nvs, NVS_DEFAULT_ELAPSED_LIMIT, &elapsed_limit);

    nvs_get_u16(nvs, NVS_DEFAULT_UNDER_POWER_LIMIT, &under_power_limit);

    pilot_set_level(true);
}

evse_state_t evse_get_state(void)
{
    return state;
}

evse_err_t evse_get_error(void)
{
    return error;
}

uint16_t evse_get_chaging_current(void)
{
    return charging_current;
}

esp_err_t evse_set_chaging_current(uint16_t value)
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

uint16_t evse_get_default_chaging_current(void)
{
    uint16_t value = board_config.max_charging_current * 10;
    nvs_get_u16(nvs, NVS_DEFAULT_CHARGING_CURRENT, &value);
    return value;
}

esp_err_t evse_set_default_chaging_current(uint16_t value)
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
        ESP_LOGE(TAG, "Cant residual current minitor not available");
        return ESP_ERR_INVALID_ARG;
    }

    rcm = _rcm;

    nvs_set_u8(nvs, NVS_RCM, rcm);
    nvs_commit(nvs);

    if (rcm && rcm_test() == false) {
        set_state(EVSE_STATE_E);
        error = EVSE_ERR_RCM_SELFTEST_FAULT;
    }

    return ESP_OK;
}

bool evse_is_rcm(void)
{
    return rcm;
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
    return evse_state_is_session(state) && require_auth && !authorized;
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
                set_state(EVSE_STATE_B);
            }
        }

        state_update();
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

uint32_t evse_get_elapsed_limit(void)
{
    return elapsed_limit;
}

void evse_set_elapsed_limit(uint32_t value)
{
    elapsed_limit = value;
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
    nvs_get_u32(nvs, NVS_DEFAULT_CONSUMTION_LIMIT, &value);
    return value;
}

void evse_set_default_consumption_limit(uint32_t value)
{
    nvs_set_u32(nvs, NVS_DEFAULT_CONSUMTION_LIMIT, value);
    nvs_commit(nvs);
}

uint32_t evse_get_default_elapsed_limit(void)
{
    uint32_t value = 0;
    nvs_get_u32(nvs, NVS_DEFAULT_ELAPSED_LIMIT, &value);
    return value;
}

void evse_set_default_elapsed_limit(uint32_t value)
{
    nvs_set_u32(nvs, NVS_DEFAULT_ELAPSED_LIMIT, value);
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