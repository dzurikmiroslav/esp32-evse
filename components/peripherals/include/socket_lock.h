#ifndef SOCKED_LOCK_H_
#define SOCKED_LOCK_H_

#include <esp_err.h>
#include <stdbool.h>

typedef enum {
    SOCKET_LOCK_STATUS_IDLE,
    SOCKET_LOCK_STATUS_OPERATING,
    SOCKET_LOCK_STATUS_LOCKING_FAIL,
    SOCKET_LOCK_STATUS_UNLOCKING_FAIL
} socket_lock_status_t;

/**
 * @brief Initialize socket lock
 *
 */
void socket_lock_init(void);

/**
 * @brief Get socket lock detection on high, stored in NVS
 *
 * @return true when locked has zero resistance
 * @return false when unlocked has zero resistance
 */
bool socket_lock_is_detection_high(void);

/**
 * @brief Set socket lock detection on high, stored in NVS
 *
 * @param detection_high
 */
void socket_lock_set_detection_high(bool detection_high);

/**
 * @brief Get socket lock operating time
 *
 * @return time in ms
 */
uint16_t socket_lock_get_operating_time(void);

/**
 * @brief Set socket lock operating time
 *
 * @param operating_time - time in ms
 * @return esp_err_t
 */
esp_err_t socket_lock_set_operating_time(uint16_t operating_time);

/**
 * @brief Get socket lock retry count
 *
 * @return retry count
 */
uint8_t socket_lock_get_retry_count(void);

/**
 * @brief Set socket lock retry count
 *
 * @param retry_count
 */
void socket_lock_set_retry_count(uint8_t retry_count);

/**
 * @brief Get socket lock break time
 *
 * @return time in ms
 */
uint16_t socket_lock_get_break_time(void);

/**
 * @brief Set socket lock break time
 *
 * @param break_time
 * @return esp_err_t
 */
esp_err_t socket_lock_set_break_time(uint16_t break_time);

/**
 * @brief Set socke lock to locked / unlocked state
 *
 * @param locked
 */
void socket_lock_set_locked(bool locked);

/**
 * @brief Get socket lock status
 *
 * @return socket_lock_status_t
 */
socket_lock_status_t socket_lock_get_status(void);

#endif /* SOCKED_LOCK_H_ */
