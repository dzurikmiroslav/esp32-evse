#ifndef DISCOVERY_
#define DISCOVERY_

#define DISCOVERY_NAME_SIZE 32

#include <esp_err.h>

/**
 * @brief Initialize net discovery
 *
 */
void discovery_init(void);

/**
 * @brief Set hotsname
 *
 * @param value
 * @return esp_err_t
 */
esp_err_t discovery_set_hostname(const char* value);

/**
 * @brief Get hostname
 *
 * @param value
 */
void discovery_get_hostname(char* value);

/**
 * @brief Set instance name
 *
 * @param value
 * @return esp_err_t
 */
esp_err_t discovery_set_instance_name(const char* value);

/**
 * @brief Get instance name
 *
 * @param value
 */
void discovery_get_instance_name(char* value);

#endif /* DISCOVERY_ */
