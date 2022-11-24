#ifndef TEMP_SENSOR_H_
#define TEMP_SENSOR_H_

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Initialize DS18S20 temperature sensor bus
 *
 */
void temp_sensor_init(void);

/**
 * @brief Get found sensor count
 * 
 * @return uint8_t 
 */
uint8_t temp_sensor_count(void);

/**
 * @brief Measure lower and higher temperature on sensor bus
 *
 * @param low 
 * @param high 
 * @return esp_err_t 
 */
esp_err_t temp_sensor_measure(int16_t *low, int16_t *high);

#endif /* TEMP_SENSOR_H_ */
