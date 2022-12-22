#ifndef REST_H_
#define REST_H_

#include <stdbool.h>

/**
 * @brief Init rest server
 * 
 */
void rest_init(void);

/**
 * @brief Set enabled rest server, stored in NVS 
 * 
 * @param enabled 
 */
void rest_set_enabled(bool enabled);

/**
 * @brief Get enabled rest server, stored in NVS 
 * 
 * @return true 
 * @return false 
 */
bool rest_is_enabled(void);

/**
 * @brief Stop rest server
 * 
 */
void rest_start(void);

/**
 * @brief Start rest server
 * 
 */
void rest_stop(void);

#endif /* REST_H_ */
