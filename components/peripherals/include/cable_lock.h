#ifndef CABLE_LOCK_H_
#define CABLE_LOCK_H_

/**
 * @brief Cable lock type
 * 
 */
typedef enum
{
    CABLE_LOCK_TYPE_NONE,
    CABLE_LOCK_TYPE_MOTOR,
    CABLE_LOCK_TYPE_SOLENOID
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
 */
void cable_lock_set_type(cable_lock_type_t type);

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

#endif /* CABLE_LOCK_H_ */
