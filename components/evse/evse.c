#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "pilot.h"
#include "relay.h"
#include "evse.h"
#include "board_config.h"


#define EVSE_MAX_CHARGING_CURRENT_MIN       6

static const char *TAG = "evse";

static SemaphoreHandle_t mutex;

static bool enabled = true;

static evse_state_t state = EVSE_STATE_A;

static evse_err_t error = EVSE_ERR_NONE;

static uint8_t error_count = 0;

static uint16_t charging_current; // 0.1A

static TickType_t session_start_tick = 0;

static uint32_t session_consumption = 0;

static uint16_t session_actual_power = 0;

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
                    session_start_tick = 0;
                    break;
                case EVSE_STATE_B:
                    ac_relay_set_state(false);
                    session_start_tick = xTaskGetTickCount();
                    break;
                case EVSE_STATE_C:
                case EVSE_STATE_D:
                    ac_relay_set_state(true);
                    break;
                case EVSE_STATE_E:
                case EVSE_STATE_F:
                    pilot_pwm_set_level(false);
                    ac_relay_set_state(false);
                    break;
            }

            state = next_state;
            state_changed = true;
        }

        // periodical update state
        switch (state) {
            case EVSE_STATE_C:
            case EVSE_STATE_D:
                pilot_pwm_set_amps(charging_current / 10);
                break;
            default:
                break;
        }

//        if (pilot_voltage == PILOT_VOLTAGE_12 && state != EVSE_STATE_A) {
//            ESP_LOGI(TAG, "Enter A state");
//
//            state = EVSE_STATE_A;
//        }
//
//        if (pilot_voltage == PILOT_VOLTAGE_9 && state != EVSE_STATE_B) {
//            ESP_LOGI(TAG, "Enter B state");
//            if (state == EVSE_STATE_A || state == EVSE_STATE_C || state == EVSE_STATE_D) {
//                state = EVSE_STATE_B;
//            } else {
//                state = EVSE_STATE_E;
//            }
//        }
//
//        if (pilot_voltage == PILOT_VOLTAGE_6 && state != EVSE_STATE_C) {
//            ESP_LOGI(TAG, "Enter C state");
//            if (state == EVSE_STATE_B || state == EVSE_STATE_D) {
//                state = EVSE_STATE_C;
//            } else {
//                state = EVSE_STATE_E;
//            }
//        }
//
//        if (pilot_voltage == PILOT_VOLTAGE_3 && state != EVSE_STATE_D) {
//            ESP_LOGI(TAG, "Enter D state");
//            if (state == EVSE_STATE_B || state == EVSE_STATE_C) {
//                state = EVSE_STATE_D;
//            } else {
//                state = EVSE_STATE_E;
//            }
//        }
//
//        if (pilot_voltage == PILOT_VOLTAGE_1 && state != EVSE_STATE_E) {
//            ESP_LOGI(TAG, "Enter E state");
//            state = EVSE_STATE_E;
//        }
//
//        switch (state) {
//            case EVSE_STATE_A:
//                pilot_pwm_set_level(true);
//                ac_relay_set_state(false);
//                break;
//            case EVSE_STATE_B:
//                pilot_pwm_set_amps(charging_current / 10);
//                ac_relay_set_state(false);
//                break;
//            case EVSE_STATE_C:
//                pilot_pwm_set_amps(charging_current / 10);
//                ac_relay_set_state(true);
//                break;
//            case EVSE_STATE_D:
//                pilot_pwm_set_amps(charging_current / 10);
//                ac_relay_set_state(true);
//                break;
//            case EVSE_STATE_E:
//                pilot_pwm_set_level(true);
//                ac_relay_set_state(false);
//                break;
//            case EVSE_STATE_F:
//                pilot_pwm_set_level(false);
//                ac_relay_set_state(false);
//                break;
//        }

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

//TODO remove
void evse_mock(evse_state_t _state)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    state = _state;

    if (state == EVSE_STATE_C || state == EVSE_STATE_D) {
        session_consumption = rand() % 40000;
        session_actual_power = 7489;
    } else {
        session_consumption = 0;
        session_actual_power = 0;
    }

    xSemaphoreGive(mutex);
}

void evse_init()
{
    mutex = xSemaphoreCreateMutex();
    charging_current = board_config.max_charging_current * 10;

    pilot_pwm_set_level(true);
}

uint32_t evse_get_session_elapsed(void)
{
    return pdTICKS_TO_MS(xTaskGetTickCount() - session_start_tick) / 1000;
}

uint32_t evse_get_session_consumption(void)
{
    return session_consumption;
}

uint16_t evse_get_session_actual_power(void)
{
    return session_actual_power;
}

evse_state_t evse_get_state(void)
{
    return state;
}

uint16_t evse_get_error(void)
{
    return error;
}

uint16_t evse_get_chaging_current(void)
{
    return charging_current;
}

void evse_set_chaging_current(uint16_t value)
{
    value = MAX(value, EVSE_MAX_CHARGING_CURRENT_MIN * 10);
    value = MIN(value, board_config.max_charging_current * 10);
    charging_current = value;

}

