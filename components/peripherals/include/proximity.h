#ifndef PROXIMITY_H_
#define PROXIMITY_H_

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Initialize proximity check
 * 
 */
void proximity_init(void);

/**
 * @brief Get proximity check enabled
 * 
 * @return true 
 * @return false 
 */
bool proximity_is_enabled(void);

/**
 * @brief Set proximity check enabled
 * 
 * @param enabled 
 * @return esp_err_t 
 */
esp_err_t proximity_set_enabled(bool enabled);

/**
 * @brief Return measured value of max current on PP
 * 
 * @return current in A 
 */
uint8_t proximity_get_max_current(void);

#endif /* PROXIMITY_H_ */
