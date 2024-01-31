#ifndef SCRIPT_H_
#define SCRIPT_H_

#include "esp_err.h"
#include "cJSON.h"

/**
 * @brief Initialize script VM
 * 
 */
void script_init(void);

/**
 * @brief Reload script VM
 * 
 */
void script_reload(void);

/**
 * @brief Set enabled, stored in NVS 
 * 
 * @param enabled 
 */
void script_set_enabled(bool enabled);

/**
 * @brief Get enabled, stored in NVS 
 * 
 * @return true 
 * @return false 
 */
bool script_is_enabled(void);

/**
 * @brief Get entries count
 * 
 * @return uint16_t 
 */
uint16_t script_output_count(void);

/**
 * @brief Read line from index, set index for reading next entry
 * 
 * @param index 
 * @param str 
 * @param v 
 * @return true When has next entry
 * @return false When no entry left
 */
bool script_output_read(uint16_t *index, char **str, uint16_t* len);

uint8_t script_get_driver_count(void);

cJSON* script_read_config(uint8_t index);


#endif /* SCRIPT_H_ */
