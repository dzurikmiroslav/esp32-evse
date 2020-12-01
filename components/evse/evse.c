#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "evse.h"
#include "board_config.h"
#include "peripherals.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define EVSE_MAX_CHARGING_CURRENT_MIN       6

static const char *TAG = "evse";

static SemaphoreHandle_t mutex;

static bool enabled = true;

static evse_state_t state = EVSE_STATE_A;

static uint16_t error;

static uint16_t charging_current; // 0.1A

static uint8_t pause_countdown = 0;

static uint32_t session_elapsed = 0;

static uint32_t session_consumption = 0;

static uint16_t session_actual_power = 0;

bool evse_process(void)
{
    bool processed = false;

    xSemaphoreTake(mutex, portMAX_DELAY);

    if (enabled) {
        if (pause_countdown > 0) {
            pause_countdown--;
            ESP_LOGI(TAG, "Pause");
            if (pause_countdown == 0) {
                pilot_pwm_set_level(true);
            }
        } else {
            pilot_voltage_t pilot_voltage = pilot_get_voltage();

            ESP_LOGI(TAG, "pilot_voltage=%d evse_state=%d", pilot_voltage, state);

            if (pilot_voltage == PILOT_VOLTAGE_12 && state != EVSE_STATE_A) {
                ESP_LOGI(TAG, "Enter A state");

                state = EVSE_STATE_A;
            }

            if (pilot_voltage == PILOT_VOLTAGE_9 && state != EVSE_STATE_B) {
                ESP_LOGI(TAG, "Enter B state");
                if (state == EVSE_STATE_A || state == EVSE_STATE_C || state == EVSE_STATE_D) {
                    state = EVSE_STATE_B;
                } else {
                    state = EVSE_STATE_E;
                }
            }

            if (pilot_voltage == PILOT_VOLTAGE_6 && state != EVSE_STATE_C) {
                ESP_LOGI(TAG, "Enter C state");
                if (state == EVSE_STATE_B || state == EVSE_STATE_D) {
                    state = EVSE_STATE_C;
                } else {
                    state = EVSE_STATE_E;
                }
            }

            if (pilot_voltage == PILOT_VOLTAGE_3 && state != EVSE_STATE_D) {
                ESP_LOGI(TAG, "Enter D state");
                if (state == EVSE_STATE_B || state == EVSE_STATE_C) {
                    state = EVSE_STATE_D;
                } else {
                    state = EVSE_STATE_E;
                }
            }

            if (pilot_voltage == PILOT_VOLTAGE_1 && state != EVSE_STATE_E) {
                ESP_LOGI(TAG, "Enter E state");
                state = EVSE_STATE_E;
            }

            switch (state) {
                case EVSE_STATE_A:
                    pilot_pwm_set_level(true);
                    ac_relay_set_state(false);
                    break;
                case EVSE_STATE_B:
                    pilot_pwm_set_amps(charging_current / 10);
                    ac_relay_set_state(false);
                    break;
                case EVSE_STATE_C:
                    pilot_pwm_set_amps(charging_current / 10);
                    ac_relay_set_state(true);
                    break;
                case EVSE_STATE_D:
                    pilot_pwm_set_amps(charging_current / 10);
                    ac_relay_set_state(true);
                    break;
                case EVSE_STATE_E:
                    pilot_pwm_set_level(false);
                    ac_relay_set_state(false);
                    pause_countdown = 5;
                    break;
                case EVSE_STATE_F:
                    pilot_pwm_set_level(false);
                    ac_relay_set_state(false);
                    pause_countdown = 5;
                    break;
            }

            processed = true;
        }
    }

    xSemaphoreGive(mutex);

    return processed;
}

void evse_enable(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    enabled = true;
    pause_countdown = 0;

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
        session_elapsed = rand() % 3600;
        session_consumption = rand() % 40000;
        session_actual_power = 7489;
    } else {
        session_elapsed = 0;
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
    return session_elapsed;
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
    ESP_LOGI(TAG, "evse_set_chaging_current %d", value);
    value = MAX(value, EVSE_MAX_CHARGING_CURRENT_MIN * 10);
    value = MIN(value, board_config.max_charging_current * 10);
    charging_current = value;

}

