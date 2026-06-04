#ifndef LOGGER_H_
#define LOGGER_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @brief Initialize logger
 *
 */
void logger_init(void);

/**
 * @brief Get log bugger lengt
 *
 * @return uint16_t
 */
uint16_t logger_log_buffer_get_count(void);

/**
 * @brief Read logs from buffer from index, set index for reading next entry
 *
 * @param index
 * @param str
 * @param v
 * @return true When has next entry
 * @return false When no entry left
 */
bool logger_log_bugger_read(uint16_t* index, char** str, uint16_t* len);

/**
 * @brief Get panic time, if no panic return 0
 *
 * @return time_t
 */
time_t logger_get_panic_time(void);

/**
 * @brief Get panic log, if no panic return NULL
 *
 * @return const char*
 */
const char* logger_get_panic_log(void);

/**
 * @brief Clear panic log & time
 *
 */
void logger_clear_panic(void);

#endif /* LOGGER_H_ */