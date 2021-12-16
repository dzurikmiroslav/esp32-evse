#include <sys/param.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "evse.h"
#include "board_config.h"
#include "pilot.h"
#include "relay.h"
#include "cable_lock.h"

#define CHARGING_CURRENT_MIN 6

static const char* TAG = "evse";

static SemaphoreHandle_t mutex;

static uint8_t disable_bits = 0;

static evse_state_t state = EVSE_STATE_A;

static evse_err_t error = EVSE_ERR_NONE;

static uint8_t error_count = 0;

static float charging_current;

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
        pilot_pwm_set_amps(charging_current);
        break;
    case EVSE_STATE_C:
    case EVSE_STATE_D:
        ac_relay_set_state(true);
        break;
    case EVSE_STATE_E:
    case EVSE_STATE_F:
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

    if (!disable_bits) {
        evse_state_t next_state = state;

        switch (state) {
        case EVSE_STATE_A:
            switch (pilot_get_voltage()) {
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
            switch (pilot_get_voltage()) {
            case PILOT_VOLTAGE_12:
                next_state = EVSE_STATE_A;
                break;
            case PILOT_VOLTAGE_9:
                // stay in current state
                break;
            case PILOT_VOLTAGE_6:
                next_state = EVSE_STATE_C;
                break;
            default:
                next_state = EVSE_STATE_E;
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
                next_state = EVSE_STATE_E;
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
                next_state = EVSE_STATE_E;
                error = EVSE_ERR_PILOT_FAULT;
            }
            break;
        case EVSE_STATE_E:
            if (error == EVSE_ERR_PILOT_FAULT) {
                if (error_count++ > 5) {
                    error_count = 0;
                    next_state = EVSE_STATE_A;
                    error = EVSE_ERR_NONE;
                }
            }
            break;
        case EVSE_STATE_F:
            break;
        }

        if (next_state != state) {
            ESP_LOGI(TAG, "Enter %c state", 'A' + next_state);
            if (error != EVSE_ERR_NONE) {
                ESP_LOGI(TAG, "Error number %d", error);
            }
            set_state(next_state);
        }
    }

    xSemaphoreGive(mutex);
}

void evse_enable(uint8_t bit)
{
    ESP_LOGI(TAG, "Enable bit %d", bit);
    xSemaphoreTake(mutex, portMAX_DELAY);

    disable_bits &= ~(1 << bit);

    xSemaphoreGive(mutex);
}

void evse_disable(uint8_t bit)
{
    ESP_LOGI(TAG, "Disable bit %d", bit);
    xSemaphoreTake(mutex, portMAX_DELAY);

    disable_bits |= (1 << bit);
    set_state(EVSE_STATE_A);

    xSemaphoreGive(mutex);
}

bool evse_is_enabled(void)
{
    return !disable_bits;
}

void evse_init()
{
    mutex = xSemaphoreCreateMutex();
    charging_current = board_config.max_charging_current;

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
    ESP_LOGI(TAG, "evse_set_chaging_current %f", value);
    xSemaphoreTake(mutex, portMAX_DELAY);

    value = MAX(value, CHARGING_CURRENT_MIN);
    value = MIN(value, board_config.max_charging_current);
    charging_current = value;

    if (state == EVSE_STATE_C || state == EVSE_STATE_D) {
        pilot_pwm_set_amps(charging_current);
    }

    xSemaphoreGive(mutex);
}
