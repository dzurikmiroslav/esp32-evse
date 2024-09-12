#ifndef LOGGER_H_
#define LOGGER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdbool.h>
#include <stdint.h>

#define LOGGER_SERIAL_BIT BIT0

/**
 * @brief Logger event group LOGGER_SERIAL_BIT
 *
 */
extern EventGroupHandle_t logger_event_group;

/**
 * @brief Initialize logger
 *
 */
void logger_init(void);

/**
 * @brief Print
 *
 * @param str
 */
void logger_print(const char* str);

/**
 * @brief Print va
 *
 * @param str
 * @param l
 * @return int
 */
int logger_vprintf(const char* str, va_list l);

/**
 * @brief Get entries count
 *
 * @return uint16_t
 */
uint16_t logger_count(void);

/**
 * @brief Read line from index, set index for reading next entry
 *
 * @param index
 * @param str
 * @param v
 * @return true When has next entry
 * @return false When no entry left
 */
bool logger_read(uint16_t* index, char** str, uint16_t* len);

#endif /* LOGGER_H_ */