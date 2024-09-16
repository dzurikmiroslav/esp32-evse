#include "socket_lock.h"

void socket_lock_set_locked(bool locked)
{}

socket_lock_status_t socket_lock_get_status(void)
{
    return SOCKET_LOCK_STATUS_IDLE;
}