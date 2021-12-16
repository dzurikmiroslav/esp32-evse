#ifndef MQTT_H_
#define MQTT_H_

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "evse.h"

#define MQTT_CONNECTED_BIT          BIT0
#define MQTT_DISCONNECTED_BIT       BIT1

extern EventGroupHandle_t mqtt_event_group;

void mqtt_init(void);

void mqtt_set_config(bool enabled, const char* server, const char* base_topic, const char* user, const char* password, uint16_t periodicity);

bool mqtt_get_enabled(void);

void mqtt_get_server(char* value); //string length 64

void mqtt_get_base_topic(char* value); //string length 32

void mqtt_get_user(char* value); //string length 32

void mqtt_get_password(char* value); //string length 64

uint16_t mqtt_get_periodicity(void);

void mqtt_process(void);

#endif /* MQTT_H_ */
