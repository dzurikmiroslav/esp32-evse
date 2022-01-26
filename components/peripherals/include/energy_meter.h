#ifndef ENERGY_METER_H_
#define ENERGY_METER_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Mode of energy meter
 *
 */
typedef enum {
    ENERGY_METER_MODE_NONE,
    ENERGY_METER_MODE_CUR,
    ENERGY_METER_MODE_CUR_VLT,
    ENERGY_METER_MODE_PULSE
    //TODO ENERGY_METER_MODE_MODBUS
} energy_meter_mode_t;

/**
 * @brief Initialize energy meter
 *
 */
void energy_meter_init(void);

/**
 * @brief Get mode of energy meter, stored in NVS
 *
 * @return energy_meter_mode_t
 */
energy_meter_mode_t energy_meter_get_mode(void);

/**
 * @brief Set mode of energy meter, stored in NVS
 * 
 * @param mode 
 */
void energy_meter_set_mode(energy_meter_mode_t mode);

/**
 * @brief Get amount Wh one pulse of pusle energy meter, stored in NVS
 * 
 * @return Wh of one pulse 
 */
uint16_t energy_meter_get_pulse_amount(void);

/**
 * @brief Set amount Wh one pulse of pusle energy meter, stored in NVS
 * 
 * @param pulse_amount Wh of one pulse
 */
void energy_meter_set_pulse_amount(uint16_t pulse_amount);

/**
 * @brief Get AC voltage, stored in NVS
 *
 * @note AC voltage is used when using internal meter and board config has not voltage sensing (BOARD_CONFIG_ENERGY_METER_NONE, BOARD_CONFIG_ENERGY_METER_CUR)
 *
 * @return Voltage in V
 */
uint16_t energy_meter_get_ac_voltage(void);

/**
 * @brief Set AC voltage, stored in NVS
 *  
 * @note AC voltage is used when using internal meter and board config has not voltage sensing (BOARD_CONFIG_ENERGY_METER_NONE, BOARD_CONFIG_ENERGY_METER_CUR)
 *
 * @param ac_voltage voltage in V
 */
void energy_meter_set_ac_voltage(uint16_t ac_voltage);

/**
 * @brief Main loop of energy meter
 *
 */
void energy_meter_process(void);

/**
 * @brief Get charging actual power
 *
 * @return Power in W
 */
uint16_t energy_meter_get_power(void);

/**
 * @brief Get charging elapsed time
 *
 * @return Time in S
 */
uint32_t energy_meter_get_session_elapsed(void);

/**
 * @brief Get charging consumption
 *
 * @return consumption in 1Ws
 */
uint32_t energy_meter_get_session_consumption(void);

/**
 * @brief After energy_meter_process, get current measured voltage
 *
 * @param voltage output array of 3 items, values in V
 */
void energy_meter_get_voltage(float* voltage);

/**
 * @brief After energy_meter_process, get current measured current
 *
 * @param voltage output array of 3 items, values in A
 */
void energy_meter_get_current(float* current);

#endif /* ENERGY_METER_H_ */
