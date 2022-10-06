#ifndef TCP_LOGGER_H_
#define TCP_LOGGER_H_

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize tcp logger
 * 
 */
void tcp_logger_init(void);

/**
 * @brief Set enabled, stored in NVS 
 * 
 * @param enabled 
 */
void tcp_logger_set_enabled(bool enabled);

/**
 * @brief Get enabled, stored in NVS 
 * 
 * @return true 
 * @return false 
 */
bool tcp_logger_is_enabled(void);

/**
 * @brief Process log
 * 
 * @param str 
 * @param len 
 */
void tcp_logger_log_process(const char *str, int len);

#endif /* TCP_LOGGER_H_ */
