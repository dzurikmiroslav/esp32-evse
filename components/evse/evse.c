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

#define EVSE_MAX_CHARGING_CURRENT_MIN       6

static const char *TAG = "evse";

static SemaphoreHandle_t mutex;

static bool enabled = true;

static evse_state_t state = EVSE_STATE_A;

static evse_err_t error = EVSE_ERR_NONE;

static uint8_t error_count = 0;

static float charging_current;

bool evse_process(void)
{
    bool state_changed = false;

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (enabled) {
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
            // apply next state settings
            switch (next_state) {
                case EVSE_STATE_A:
                    pilot_pwm_set_level(true);
                    ac_relay_set_state(false);
                    cable_lock_unlock();
                    break;
                case EVSE_STATE_B:
                    ac_relay_set_state(false);
                    cable_lock_lock();
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
            state_changed = true;
        }

        switch (state) {
            case EVSE_STATE_C:
            case EVSE_STATE_D:
                pilot_pwm_set_amps(charging_current);
                break;
            default:
                break;
        }
    }

    xSemaphoreGive(mutex);

    return state_changed;
}

void evse_enable(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    enabled = true;

    xSemaphoreGive(mutex);
}

void evse_disable(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    enabled = false;
    pilot_pwm_set_level(true);

    xSemaphoreGive(mutex);
}

bool evse_try_disable(void)
{
    bool sucess = false;

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (state != EVSE_STATE_B && state != EVSE_STATE_C && state != EVSE_STATE_D) {
        enabled = false;
        pilot_pwm_set_level(true);

        sucess = true;
    }

    xSemaphoreGive(mutex);

    return sucess;
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
    value = MAX(value, EVSE_MAX_CHARGING_CURRENT_MIN);
    value = MIN(value, board_config.max_charging_current);
    charging_current = value;
}

