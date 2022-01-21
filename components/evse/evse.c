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
#include "ac_relay.h"
#include "cable_lock.h"

#define CHARGING_CURRENT_MIN            6
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

static float charging_current;

static bool pause = false;

static bool require_auth = false;

static bool pending_auth = false;

static TickType_t authorized_to = 0;

static TickType_t error_wait_to = 0;

void set_state(evse_state_t next_state)
{
    switch (next_state) {
    case EVSE_STATE_A:
        pilot_pwm_set_level(true);
        ac_relay_set_state(false);
        cable_lock_unlock();
        break;
    case EVSE_STATE_B:
        ac_relay_set_state(false);
        cable_lock_lock();
        if (pause) {
            pilot_pwm_set_level(true);
        } else {
            pilot_pwm_set_amps(charging_current);
        }
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
        pilot_pwm_set_level(false);
        ac_relay_set_state(false);
        cable_lock_unlock();
        break;
    }

    state = next_state;
}

void evse_process(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    pilot_measure();
    evse_state_t next_state = state;

    if (!pilot_test_diode()) {
        next_state = EVSE_STATE_ERROR;
        error = EVSE_ERR_DIODE_SHORT;
    } else {
        switch (state) {
        case EVSE_STATE_A:
            switch (pilot_get_voltage()) {
            case PILOT_VOLTAGE_12:
                // stay in current state
                pending_auth = false;
                break;
            case PILOT_VOLTAGE_9:
                if (require_auth) {
                    if (authorized_to >= xTaskGetTickCount()) {
                        next_state = EVSE_STATE_B;
                        pending_auth = false;
                        authorized_to = 0;
                    } else {
                        pending_auth = true;
                    }
                } else {
                    next_state = EVSE_STATE_B;
                }
                break;
            default:
                next_state = EVSE_STATE_ERROR;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_B:
            switch (pilot_get_voltage()) {
            case PILOT_VOLTAGE_12:
                next_state = EVSE_STATE_A;
                break;
            case PILOT_VOLTAGE_9:
                // stay in current state
                break;
            case PILOT_VOLTAGE_6:
                if (!pause) {
                    next_state = EVSE_STATE_C;
                }
                break;
            default:
                next_state = EVSE_STATE_ERROR;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_C:
            switch (pilot_get_voltage()) {
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
            switch (pilot_get_voltage()) {
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
        ESP_LOGI(TAG, "Enter %c state", 'A' + next_state);
        if (error != EVSE_ERR_NONE) {
            ESP_LOGI(TAG, "Error number %d", error);
        }
        set_state(next_state);
    }

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

    charging_current = board_config.max_charging_current;
    size_t size = sizeof(float);
    nvs_get_blob(nvs, NVS_DEFAULT_CHARGING_CURRENT, &charging_current, &size);

    uint8_t u8;
    if (nvs_get_u8(nvs, NVS_REQUIRE_AUTH, &u8) == ESP_OK) {
        require_auth = u8;
    }

    pilot_pwm_set_level(true);
}

evse_state_t evse_get_state(void)
{
    return state;
}

uint16_t evse_get_error(void)
{
    return error;
}

float evse_get_chaging_current(void)
{
    return charging_current;
}

void evse_set_chaging_current(float value)
{
    ESP_LOGI(TAG, "Set charging current %f", value);
    xSemaphoreTake(mutex, portMAX_DELAY);

    value = MAX(value, CHARGING_CURRENT_MIN);
    value = MIN(value, board_config.max_charging_current);
    charging_current = value;

    if (evse_state_is_session(state)) {
        pilot_pwm_set_amps(charging_current);
    }

    xSemaphoreGive(mutex);
}

float evse_get_default_chaging_current(void)
{
    float value = board_config.max_charging_current;
    size_t size = sizeof(float);
    nvs_get_blob(nvs, NVS_DEFAULT_CHARGING_CURRENT, &value, &size);
    return value;
}

void evse_set_default_chaging_current(float value)
{
    ESP_LOGI(TAG, "Set default charging current %f", value);

    nvs_set_blob(nvs, NVS_DEFAULT_CHARGING_CURRENT, (void*)&value, sizeof(float));
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
    authorized_to = xTaskGetTickCount() + pdMS_TO_TICKS(AUTHORIZED_TIME);
}

bool evse_is_pending_auth(void)
{
    return pending_auth;
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
    } else {
        set_state(state);
    }

    xSemaphoreGive(mutex);
}

void evse_unpause(void)
{
    ESP_LOGI(TAG, "Unpause");
    xSemaphoreTake(mutex, portMAX_DELAY);

    pause = false;
    set_state(state);

    xSemaphoreGive(mutex);
}