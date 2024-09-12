#ifndef SERIAL_H_
#define SERIAL_H_

#include <driver/uart.h>
#include <esp_err.h>

/**
 * @brief Serial identifiers
 *
 */
typedef enum {
    SERIAL_ID_1,
    SERIAL_ID_2,
#if SOC_UART_NUM > 2
    SERIAL_ID_3,
#endif
    SERIAL_ID_MAX
} serial_id_t;

/**
 * @brief Serial modes
 *
 */
typedef enum {
    SERIAL_MODE_NONE,
    SERIAL_MODE_LOG,
    SERIAL_MODE_MODBUS,
    SERIAL_MODE_NEXTION,
    SERIAL_MODE_MAX
} serial_mode_t;

/**
 * @brief Initialize serial
 *
 */
void serial_init(void);

/**
 * @brief Check if serial is availabale
 *
 * @param id
 * @return true
 * @return false
 */
bool serial_is_available(serial_id_t id);

/**
 * @brief Get serial mode
 *
 * @param id serial id
 * @return serial_mode_t
 */
serial_mode_t serial_get_mode(serial_id_t id);

/**
 * @brief Get baud rate
 *
 * @param id serial id
 * @return int
 */
int serial_get_baud_rate(serial_id_t id);

/**
 * @brief Get data bits
 *
 * @param id serial id
 * @return uart_word_length_t
 */
uart_word_length_t serial_get_data_bits(serial_id_t id);

/**
 * @brief Get stop bits
 *
 * @param id serial id
 * @return uart_stop_bits_t
 */
uart_stop_bits_t serial_get_stop_bits(serial_id_t id);

/**
 * @brief Get parity
 *
 * @param id serial id
 * @return uart_parity_t
 */
uart_parity_t serial_get_parity(serial_id_t id);

/**
 * @brief Reseal all serials config, before calling serial_set_config
 *
 */
void serial_reset_config(void);

/**
 * @brief Set serial config
 *
 * @param id
 * @param mode
 * @param baud_rate
 * @param data_bits
 * @param stop_bits
 * @param parity
 * @return esp_err_t
 */

esp_err_t serial_set_config(serial_id_t id, serial_mode_t mode, int baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bits, uart_parity_t parity);

/**
 * @brief Format to string value
 *
 * @param mode
 * @return const char*
 */
const char* serial_mode_to_str(serial_mode_t mode);

/**
 * @brief Parse from string value
 *
 * @param str
 * @return serial_mode_t
 */
serial_mode_t serial_str_to_mode(const char* str);

/**
 * @brief Format to string value
 *
 * @param bits
 * @return const char*
 */
const char* serial_data_bits_to_str(uart_word_length_t bits);

/**
 * @brief Parse from string value
 *
 * @param str
 * @return uart_word_length_t
 */
uart_word_length_t serial_str_to_data_bits(const char* str);

/**
 * @brief Format to string value
 *
 * @param bits
 * @return const char*
 */
const char* serial_stop_bits_to_str(uart_stop_bits_t bits);

/**
 * @brief Parse from string value
 *
 * @param str
 * @return uart_stop_bits_t
 */
uart_stop_bits_t serial_str_to_stop_bits(const char* str);

/**
 * @brief Format to string value
 *
 * @param parity
 * @return const char*
 */
const char* serial_parity_to_str(uart_parity_t parity);

/**
 * @brief Parse from string value
 *
 * @param str
 * @return uart_parity_t
 */
uart_parity_t serial_str_to_parity(const char* str);

#endif /* SERIAL_H_ */
