#ifndef AUX_IO_H_
#define AUX_IO_H_

#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Initialize aux
 *
 */
void aux_init(void);

/**
 * @brief Read digital input
 *
 * @param name
 * @param value
 * @return esp_err_t
 */
esp_err_t aux_read(const char *name, bool *value);

/**
 * @brief Write digial output
 *
 * @param name
 * @param value
 * @return esp_err_t
 */
esp_err_t aux_write(const char *name, bool value);

/**
 * @brief Read analog input
 *
 * @param name
 * @param value
 * @return esp_err_t
 */
esp_err_t aux_analog_read(const char *name, int *value);

#endif /* AUX_IO_H_ */