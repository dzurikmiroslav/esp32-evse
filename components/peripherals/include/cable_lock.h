#ifndef CABLE_LOCK_H_
#define CABLE_LOCK_H_

typedef enum
{
    CABLE_LOCK_TYPE_NONE,
    CABLE_LOCK_TYPE_MOTOR,
    CABLE_LOCK_TYPE_SOLENOID
} cable_lock_type_t;

void cable_lock_init(void);

cable_lock_type_t cable_lock_get_type();

void cable_lock_set_type(cable_lock_type_t type);

void cable_lock_lock(void);

void cable_lock_unlock(void);

#endif /* CABLE_LOCK_H_ */
