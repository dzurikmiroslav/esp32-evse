#ifndef EVSE_H_
#define EVSE_H_

#define EVSE_MAX_CHARGING_CURRENT_DEFAULT   16
#define EVSE_MAX_CHARGING_CURRENT_MIN       6
#define EVSE_MAX_CHARGING_CURRENT_MAX       80
#define EVSE_CABLE_LOCK_NONE                0
#define EVSE_CABLE_LOCK_MOTOR               1
#define EVSE_CABLE_LOCK_SELENOID            2

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum
{
    EVSE_STATE_A, EVSE_STATE_B, EVSE_STATE_C, EVSE_STATE_D, EVSE_STATE_E, EVSE_STATE_F
} evse_state_t;

typedef enum
{
    EVSE_EVENT_STATE_CHANGED, EVSE_EVENT_CURRNET_VOLTAGE_CHANGED, EVSE_EVENT_MAX_CHARGING_CURRENT_CHANGED, EVSE_EVENT_CHARGING_CURRENT_CHANGED
} evse_event_t;

extern QueueHandle_t evse_event_queue;

void evse_init();

evse_state_t evse_get_state();

uint16_t evse_get_error();

/**
 * Get maximum charging current, stored in NVS (unit 1A)
 */
uint8_t evse_get_max_chaging_current();

/**
 * Set maximum charging current, stored in NVS (unit 1A)
 */
void evse_set_max_chaging_current(uint8_t max_charging_current);

/**
 * Get charging current (unit 0.1A)
 */
uint16_t evse_get_chaging_current();

/**
 * Set charging current (unit 0.1A)
 */
void evse_set_chaging_current(uint16_t charging_current);

/**
 * Get cable lock
 */
uint8_t evse_get_cable_lock();

/**
 * Set cable lock
 */
void evse_set_cable_lock(uint8_t cable_lock);

/**
 * Get current measurement
 */
float evse_get_l1_current();
float evse_get_l2_current();
float evse_get_l3_current();

/**
 * Get volgate measurement
 */
float evse_get_l1_voltage();
float evse_get_l2_voltage();
float evse_get_l3_voltage();

#endif /* EVSE_H_ */
