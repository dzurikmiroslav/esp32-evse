#ifndef CABLE_LOCK_H_
#define CABLE_LOCK_H_

#include "esp_err.h"

/**
 * @brief Cable lock type
 * 
 */
typedef enum
{
    CABLE_LOCK_TYPE_NONE,
    CABLE_LOCK_TYPE_MOTOR,
    CABLE_LOCK_TYPE_SOLENOID,
    CABLE_LOCK_TYPE_MAX
} cable_lock_type_t;

/**
 * @brief Initialize cable lock
 * 
 */
void cable_lock_init(void);

/**
 * @brief Get cable lock type, stored in NVS
 * 
 * @return cable_lock_type_t 
 */
cable_lock_type_t cable_lock_get_type(void);

/**
 * @brief Set cable lock type, stored in NVS
 * 
 * @param type 
 * @return esp_err_t 
 */
esp_err_t cable_lock_set_type(cable_lock_type_t type);

/**
 * @brief Lock
 * 
 */
void cable_lock_lock(void);

/**
 * @brief Unlock
 * 
 */
void cable_lock_unlock(void);

/**
 * @brief Serialize to string
 * 
 * @param type 
 * @return const char* 
 */
const char* cable_lock_type_to_str(cable_lock_type_t type);

/**
 * @brief Parse from string
 * 
 * @param str 
 * @return cable_lock_type_t 
 */
cable_lock_type_t cable_lock_str_to_type(const char* str);

#endif /* CABLE_LOCK_H_ */
