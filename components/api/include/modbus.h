#ifndef MODBUS_H_
#define MODBUS_H_

#include "esp_err.h"

/**
 * @brief Initialize modbus
 * 
 */
void modbus_init(void);

/**
 * @brief Modbus main loop
 * 
 */
void modbus_process(void);

/**
 * @brief Get modbus unit id
 * 
 * @return uint8_t 
 */
uint8_t modbus_get_unit_id(void);

/**
 * @brief Get modbus tcp is enabled 
 * 
 * @return true 
 * @return false 
 */
bool modbus_is_tcp_enabled(void);

/**
 * @brief Set modbus unit id
 * 
 * @param unit_id 
 */
esp_err_t modbus_set_unit_id(uint8_t unit_id);

/**
 * @brief Set modbus tcp enabled
 * 
 * @param tcp_enabled 
 */
void modbus_set_tcp_enabled(bool tcp_enabled);

#endif /* MODBUS_H_ */
