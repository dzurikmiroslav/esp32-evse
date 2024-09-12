#ifndef ENERGY_METER_H_
#define ENERGY_METER_H_

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Mode of energy meter
 *
 */
typedef enum {
    ENERGY_METER_MODE_DUMMY,
    ENERGY_METER_MODE_CUR,
    ENERGY_METER_MODE_CUR_VLT,
    // TODO ENERGY_METER_MODE_MODBUS
    ENERGY_METER_MODE_MAX
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
 * @return esp_err_t
 */
esp_err_t energy_meter_set_mode(energy_meter_mode_t mode);

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
 * @return esp_err_t
 */
esp_err_t energy_meter_set_ac_voltage(uint16_t ac_voltage);

/**
 * @brief Is three phase energy meter, stored in NVS
 *
 * @return true
 * @return false
 */
bool energy_meter_is_three_phases(void);

/**
 * @brief Set three phase energy meter, stored in NVS
 *
 * @param three_phases
 */
void energy_meter_set_three_phases(bool three_phases);

/**
 * @brief Start energy meter session, if not started
 *
 */
void energy_meter_start_session(void);

/**
 * @brief Stop energy meter session, if not stopped
 *
 */
void energy_meter_stop_session(void);

/**
 * @brief Process energy meter
 *
 * @param charging
 * @param charging_current
 */
void energy_meter_process(bool charging, uint16_t charging_current);

/**
 * @brief Get session actual power
 *
 * @return Power in W
 */
uint16_t energy_meter_get_power(void);

/**
 * @brief Get session time
 *
 * @return Time in s
 */
uint32_t energy_meter_get_session_time(void);

/**
 * @brief Get charging time
 *
 * @return Time in s
 */
uint32_t energy_meter_get_charging_time(void);

/**
 * @brief Get session consumption
 *
 * @return Consumption in Wh
 */
uint32_t energy_meter_get_consumption(void);

/**
 * @brief After energy_meter_process, get current measured voltage
 *
 * @param voltage Output array of 3 items, values in V
 */
void energy_meter_get_voltage(float* voltage);

/**
 * @brief Cet current measured voltage on L1
 *
 * @return Voltage in V
 */
float energy_meter_get_l1_voltage(void);

/**
 * @brief Cet current measured voltage on L2
 *
 * @return Voltage in V
 */
float energy_meter_get_l2_voltage(void);

/**
 * @brief Cet current measured voltage on L3
 *
 * @return Voltage in V
 */
float energy_meter_get_l3_voltage(void);

/**
 * @brief After energy_meter_process, get current measured current
 *
 * @param voltage Output array of 3 items, values in A
 */
void energy_meter_get_current(float* current);

/**
 * @brief Cet current measured current on L1
 *
 * @return Voltage in V
 */
float energy_meter_get_l1_current(void);

/**
 * @brief Cet current measured current on L2
 *
 * @return Voltage in V
 */
float energy_meter_get_l2_current(void);

/**
 * @brief Cet current measured current on L3
 *
 * @return Voltage in V
 */
float energy_meter_get_l3_current(void);

/**
 * @brief Serialize to string
 *
 * @param mode
 * @return const char*
 */
const char* energy_meter_mode_to_str(energy_meter_mode_t mode);

/**
 * @brief Parse from string
 *
 * @param str
 * @return energy_meter_mode_t
 */
energy_meter_mode_t energy_meter_str_to_mode(const char* str);

#endif /* ENERGY_METER_H_ */
