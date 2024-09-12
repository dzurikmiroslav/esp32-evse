#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <stdbool.h>

/**
 * @brief Init modbus tcp
 *
 */
void modbus_tcp_init(void);

/**
 * @brief Set enabled, stored in NVS
 *
 * @param enabled
 */
void modbus_tcp_set_enabled(bool enabled);

/**
 * @brief Get enabled, stored in NVS
 *
 * @return true
 * @return false
 */
bool modbus_tcp_is_enabled(void);

#endif /* MODBUS_TCP_H */