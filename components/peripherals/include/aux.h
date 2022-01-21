#ifndef AUX_H_
#define AUX_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum
{
    AUX_ID_1,
    AUX_ID_2,
    AUX_ID_3,
    AUX_ID_MAX
} aux_id_t;

typedef enum
{
    AUX_MODE_NONE,
    AUX_MODE_PAUSE_BUTTON,
    AUX_MODE_PAUSE_SWITCH,
    AUX_MODE_UNPAUSE_SWITCH,
    AUX_MODE_AUTHORIZE_BUTTON,
    AUX_MODE_PULSE_ENERGY_METER
} aux_mode_t;

extern SemaphoreHandle_t aux_pulse_energy_meter_semhr;

void aux_init(void);

void aux_process(void);

aux_mode_t aux_get_mode(aux_id_t id);

void aux_set_mode(aux_id_t id, aux_mode_t mode);

#endif /* AUX_H_ */
