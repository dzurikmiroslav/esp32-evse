#ifndef AUX_H_
#define AUX_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief Aux identifiers
 * 
 */
typedef enum
{
    AUX_ID_1,
    AUX_ID_2,
    AUX_ID_3,
    AUX_ID_MAX
} aux_id_t;


/**
 * @brief Aux modes
 * 
 */
typedef enum
{
    AUX_MODE_NONE,
    AUX_MODE_PAUSE_BUTTON,
    AUX_MODE_PAUSE_SWITCH,
    AUX_MODE_UNPAUSE_SWITCH,
    AUX_MODE_AUTHORIZE_BUTTON,
    AUX_MODE_PULSE_ENERGY_METER
} aux_mode_t;

/**
 * @brief Output of pulse energy meter in mode AUX_MODE_PULSE_ENERGY_METER
 * 
 */
extern SemaphoreHandle_t aux_pulse_energy_meter_semhr;

/**
 * @brief Initialize aux
 * 
 */
void aux_init(void);

/**
 * @brief Main loop of aux
 * 
 */
void aux_process(void);

/**
 * @brief Return aux mode, stored in NVS
 * 
 * @param id 
 * @return aux_mode_t 
 */
aux_mode_t aux_get_mode(aux_id_t id);

/**
 * @brief Set aux mode, stored in NVS
 * 
 * @param id 
 * @param mode 
 */
void aux_set_mode(aux_id_t id, aux_mode_t mode);

#endif /* AUX_H_ */
