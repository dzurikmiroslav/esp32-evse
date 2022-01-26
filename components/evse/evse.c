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
#include "cable_lock.h"

#define CHARGING_CURRENT_MIN            60
#define AUTHORIZED_TIME                 60000 // 60sec
#define ERROR_WAIT_TIME                 10000 // 10sec TODO move up

#define NVS_NAMESPACE                   "evse"
#define NVS_DEFAULT_CHARGING_CURRENT    "def_chrg_curr"
#define NVS_REQUIRE_AUTH                "require_auth"

static const char* TAG = "evse";

static nvs_handle nvs;

static SemaphoreHandle_t mutex;

static evse_state_t state = EVSE_STATE_A;

static evse_err_t error = EVSE_ERR_NONE;

static uint16_t charging_current;

static bool pause = false;

static bool require_auth = false;

static bool authorized = false;

static TickType_t auth_grant_to = 0;

static TickType_t error_wait_to = 0;

static uint8_t cable_max_current = 0;

enum pilot_state_e {
    PILOT_STATE_12V,
    PILOT_STATE_N12V,
    PILOT_STATE_PWM
};

static enum pilot_state_e pilot_state = PILOT_STATE_12V;


void set_pilot(enum pilot_state_e state) {
    if (state != pilot_state) {
        pilot_state = state;
        switch (pilot_state) {
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

void set_state(evse_state_t next_state)
{
    switch (next_state) {
    case EVSE_STATE_A:
        set_pilot(PILOT_STATE_12V);
        ac_relay_set_state(false);
        cable_lock_unlock();
        break;
    case EVSE_STATE_B:
        cable_max_current = proximity_get_max_current();
        ac_relay_set_state(false);
        cable_lock_lock();
        break;
    case EVSE_STATE_C:
    case EVSE_STATE_D:
        ac_relay_set_state(true);
        break;
    case EVSE_STATE_ERROR:
    case EVSE_STATE_DISABLED:
        if (next_state == EVSE_STATE_ERROR) {
            error_wait_to = xTaskGetTickCount() + pdMS_TO_TICKS(ERROR_WAIT_TIME);
        }
        pilot_set_level(false);
        set_pilot(PILOT_STATE_N12V);
        cable_lock_unlock();
        break;
    }

    ESP_LOGI(TAG, "Enter %c state", 'A' + next_state);
    if (error != EVSE_ERR_NONE) {
        ESP_LOGI(TAG, "Error number %d", error);
    }
    state = next_state;
}

void state_update(void)
{
    switch (state) {
    case EVSE_STATE_A:
        authorized = false;
        break;
    case EVSE_STATE_B:
        if (!authorized && require_auth) {
            authorized = auth_grant_to >= xTaskGetTickCount();
            auth_grant_to = 0;
        }
        if (pause || (require_auth && !authorized)) {
            set_pilot(PILOT_STATE_12V);
        } else {
            set_pilot(PILOT_STATE_PWM);
        }
        break;
    case EVSE_STATE_C:
    case EVSE_STATE_D:
        break;
    case EVSE_STATE_ERROR:
    case EVSE_STATE_DISABLED:
        authorized = false;
        break;
    }
}

void evse_process(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    pilot_measure();
    evse_state_t next_state = state;

    if (pilot_state == PILOT_STATE_PWM && !pilot_is_down_voltage_n12()) {
        next_state = EVSE_STATE_ERROR;
        error = EVSE_ERR_DIODE_SHORT;
    } else {
        switch (state) {
        case EVSE_STATE_A:
            switch (pilot_get_up_voltage()) {
            case PILOT_VOLTAGE_12:
                // stay in current state
                break;
            case PILOT_VOLTAGE_9:
                next_state = EVSE_STATE_B;
                break;
            default:
                next_state = EVSE_STATE_ERROR;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_B:
            switch (pilot_get_up_voltage()) {
            case PILOT_VOLTAGE_12:
                next_state = EVSE_STATE_A;
                break;
            case PILOT_VOLTAGE_9:
                // stay in current state
                break;
            case PILOT_VOLTAGE_6:
                if (!pause && (authorized || !require_auth)) {
                    next_state = EVSE_STATE_C;
                }
                break;
            default:
                next_state = EVSE_STATE_ERROR;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_C:
            switch (pilot_get_up_voltage()) {
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
                next_state = EVSE_STATE_ERROR;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_D:
            switch (pilot_get_up_voltage()) {
            case PILOT_VOLTAGE_6:
                next_state = EVSE_STATE_C;
                break;
            case PILOT_VOLTAGE_3:
                // stay in current state
                break;
            default:
                next_state = EVSE_STATE_ERROR;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_ERROR:
            if (xTaskGetTickCount() > error_wait_to) {
                next_state = EVSE_STATE_A;
                error = EVSE_ERR_NONE;
            }
            break;
        case EVSE_STATE_DISABLED:
            break;
        }
    }

    if (next_state != state) {
        set_state(next_state);
    }

    state_update();

    xSemaphoreGive(mutex);
}

void evse_enable(void)
{
    ESP_LOGI(TAG, "Enable");
    xSemaphoreTake(mutex, portMAX_DELAY);

    set_state(EVSE_STATE_A);

    xSemaphoreGive(mutex);
}

void evse_disable(void)
{
    ESP_LOGI(TAG, "Disable");
    xSemaphoreTake(mutex, portMAX_DELAY);

    set_state(EVSE_STATE_DISABLED);

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

void evse_set_chaging_current(uint16_t value)
{
    ESP_LOGI(TAG, "Set charging current %dA*10", value);
    xSemaphoreTake(mutex, portMAX_DELAY);

    value = MAX(value, CHARGING_CURRENT_MIN);
    value = MIN(value, board_config.max_charging_current * 10);
    charging_current = value;

    if (pilot_state == PILOT_STATE_PWM) {
        pilot_set_amps(MIN(charging_current, cable_max_current * 10));
    }

    xSemaphoreGive(mutex);
}

uint16_t evse_get_default_chaging_current(void)
{
    uint16_t value = board_config.max_charging_current * 10;
    nvs_get_u16(nvs, NVS_DEFAULT_CHARGING_CURRENT, &value);
    return value;
}

void evse_set_default_chaging_current(uint16_t value)
{
    ESP_LOGI(TAG, "Set default charging current %dA*10", value);

    nvs_set_u16(nvs, NVS_DEFAULT_CHARGING_CURRENT, value);
    nvs_commit(nvs);
}

bool evse_is_require_auth(void)
{
    return require_auth;
}

void evse_set_require_auth(bool value)
{
    ESP_LOGI(TAG, "Set require auth %d", value);

    require_auth = value;

    nvs_set_u8(nvs, NVS_REQUIRE_AUTH, require_auth);
    nvs_commit(nvs);
}

void evse_authorize(void)
{
    ESP_LOGI(TAG, "Authorize");
    auth_grant_to = xTaskGetTickCount() + pdMS_TO_TICKS(AUTHORIZED_TIME);
}

bool evse_is_pending_auth(void)
{
    return evse_state_is_session(state) && require_auth && !authorized;
}

bool evse_is_paused(void)
{
    return pause;
}

void evse_pause(void)
{
    ESP_LOGI(TAG, "Pause");
    xSemaphoreTake(mutex, portMAX_DELAY);

    pause = true;
    if (evse_state_is_charging(state)) {
        set_state(EVSE_STATE_B);
    }
    state_update();

    xSemaphoreGive(mutex);
}

void evse_unpause(void)
{
    ESP_LOGI(TAG, "Unpause");
    xSemaphoreTake(mutex, portMAX_DELAY);

    pause = false;
    state_update();

    xSemaphoreGive(mutex);
}