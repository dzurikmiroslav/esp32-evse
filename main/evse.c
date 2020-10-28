#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "evse.h"
#include "peripherals.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define NVS_MAX_CHARGING_CURRENT    "max_current"
#define NVS_CABLE_LOCK              "cable_lock"

static const char *TAG = "evse";

static evse_state_t evse_state = EVSE_STATE_A;

static uint16_t evse_error;

static float evse_voltage[3] = { 0, 0, 0 };

static float evse_current[3] = { 0, 0, 0 };

static uint8_t evse_max_charging_current = EVSE_MAX_CHARGING_CURRENT_DEFAULT;  // 1A

static uint16_t evse_charging_current; // 0.1A

static uint8_t evse_cable_lock = EVSE_CABLE_LOCK_NONE;

QueueHandle_t evse_event_queue;

static TimerHandle_t evse_timer;

extern nvs_handle_t nvs;

static uint8_t pause_countdown = 0;

static void evse_timer_fn(xTimerHandle ev)
{
    if (pause_countdown > 0) {
        pause_countdown--;
        ESP_LOGI(TAG, "Pause");
        if (pause_countdown == 0) {
            pilot_pwm_set_level(true);
        }
    } else {
        pilot_voltage_t pilot_voltage = pilot_get_voltage();

        ESP_LOGI(TAG, "pilot_voltage=%d evse_state=%d", pilot_voltage, evse_state);

        if (pilot_voltage == PILOT_VOLTAGE_12 && evse_state != EVSE_STATE_A) {
            ESP_LOGI(TAG, "Enter A state");

            evse_state = EVSE_STATE_A;
        }

        if (pilot_voltage == PILOT_VOLTAGE_9 && evse_state != EVSE_STATE_B) {
            ESP_LOGI(TAG, "Enter B state");
            if (evse_state == EVSE_STATE_A || evse_state == EVSE_STATE_C || evse_state == EVSE_STATE_D) {
                evse_state = EVSE_STATE_B;
            } else {
                evse_state = EVSE_STATE_E;
            }
        }

        if (pilot_voltage == PILOT_VOLTAGE_6 && evse_state != EVSE_STATE_C) {
            ESP_LOGI(TAG, "Enter C state");
            if (evse_state == EVSE_STATE_B || evse_state == EVSE_STATE_D) {
                evse_state = EVSE_STATE_C;
            } else {
                evse_state = EVSE_STATE_E;
            }
        }

        if (pilot_voltage == PILOT_VOLTAGE_3 && evse_state != EVSE_STATE_D) {
            ESP_LOGI(TAG, "Enter D state");
            if (evse_state == EVSE_STATE_B || evse_state == EVSE_STATE_C) {
                evse_state = EVSE_STATE_D;
            } else {
                evse_state = EVSE_STATE_E;
            }
        }

        if (pilot_voltage == PILOT_VOLTAGE_1 && evse_state != EVSE_STATE_E) {
            ESP_LOGI(TAG, "Enter E state");
            evse_state = EVSE_STATE_E;
        }

        switch (evse_state) {
            case EVSE_STATE_A:
                pilot_pwm_set_level(true);
                ac_relay_set_state(false);
                break;
            case EVSE_STATE_B:
                pilot_pwm_set_amps(evse_charging_current / 10);
                ac_relay_set_state(false);
                break;
            case EVSE_STATE_C:
                pilot_pwm_set_amps(evse_charging_current / 10);
                ac_relay_set_state(true);
                break;
            case EVSE_STATE_D:
                pilot_pwm_set_amps(evse_charging_current / 10);
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
    }
}

void evse_init()
{
    evse_event_queue = xQueueCreate(5, sizeof(evse_event_t));

    nvs_get_u8(nvs, NVS_MAX_CHARGING_CURRENT, &evse_max_charging_current);
    evse_charging_current = evse_max_charging_current * 10;

    nvs_get_u8(nvs, NVS_CABLE_LOCK, &evse_cable_lock);

    evse_timer = xTimerCreate("evse_timer", pdMS_TO_TICKS(5000), pdTRUE, (void*) 0, evse_timer_fn);

    pilot_pwm_set_level(true);

    xTimerStart(evse_timer, 0);
}

evse_state_t evse_get_state()
{
    return evse_state;
}

uint16_t evse_get_error()
{
    return evse_error;
}

uint8_t evse_get_max_chaging_current()
{
    return evse_max_charging_current;
}

void evse_set_max_chaging_current(uint8_t max_charging_current)
{
    ESP_LOGI(TAG, "evse_set_max_chaging_current %d", max_charging_current);
    max_charging_current = MAX(max_charging_current, EVSE_MAX_CHARGING_CURRENT_MIN);
    max_charging_current = MIN(max_charging_current, EVSE_MAX_CHARGING_CURRENT_MAX);
    evse_max_charging_current = max_charging_current;
    nvs_set_u8(nvs, NVS_MAX_CHARGING_CURRENT, evse_max_charging_current);
    nvs_commit(nvs);

    if (evse_charging_current > evse_max_charging_current * 10) {
        evse_set_chaging_current(evse_max_charging_current * 10);
    }

    evse_event_t event = EVSE_EVENT_MAX_CHARGING_CURRENT_CHANGED;
    xQueueSend(evse_event_queue, &event, 0);
}

uint16_t evse_get_chaging_current()
{
    return evse_charging_current;
}

void evse_set_chaging_current(uint16_t charging_current)
{
    ESP_LOGI(TAG, "evse_set_chaging_current %d", charging_current);
    charging_current = MAX(charging_current, EVSE_MAX_CHARGING_CURRENT_MIN * 10);
    charging_current = MIN(charging_current, evse_max_charging_current * 10);
    evse_charging_current = charging_current;

    evse_event_t event = EVSE_EVENT_CHARGING_CURRENT_CHANGED;
    xQueueSend(evse_event_queue, &event, 0);
}

uint8_t evse_get_cable_lock()
{
    return evse_cable_lock;
}

void evse_set_cable_lock(uint8_t cable_lock)
{
    cable_lock = MAX(cable_lock, EVSE_CABLE_LOCK_SELENOID);
    evse_cable_lock = cable_lock;
    nvs_set_u8(nvs, NVS_CABLE_LOCK, evse_cable_lock);
    nvs_commit(nvs);
}

float evse_get_l1_current()
{
    return evse_current[0];
}

float evse_get_l2_current()
{
    return evse_current[1];
}

float evse_get_l3_current()
{
    return evse_current[2];
}

float evse_get_l1_voltage()
{
    return evse_voltage[0];
}

float evse_get_l2_voltage()
{
    return evse_voltage[1];
}

float evse_get_l3_voltage()
{
    return evse_voltage[2];
}

