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
uint8_t temp_sensor_get_count(void);

/**
 * @brief Return lowest temperature after temp_sensor_measure
 * 
 * @return int16_t 
 */
int16_t temp_sensor_get_low(void);

/**
 * @brief Return highest temperature after temp_sensor_measure
 * 
 * @return int16_t 
 */
int16_t temp_sensor_get_high(void);

/**
 * @brief Return last measurement error if occurred
 * 
 * @return esp_err_t 
 */
esp_err_t temp_sensor_get_error(void);

#endif /* TEMP_SENSOR_H_ */
