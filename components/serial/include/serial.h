#ifndef SERIAL_H_
#define SERIAL_H_

#include <driver/uart.h>
#include <esp_err.h>

/**
 * @brief Initialize serial
 *
 */
void serial_init(void);

/**
 * @brief Get serial mode
 *
 * @param port uart number
 * @return const char*
 */
const char* serial_get_mode(uart_port_t port);

/**
 * @brief Get baud rate
 *
 * @param port uart number
 * @return int
 */
int serial_get_baud_rate(uart_port_t port);

/**
 * @brief Get data bits
 *
 * @param port uart number
 * @return uart_word_length_t
 */
uart_word_length_t serial_get_data_bits(uart_port_t port);

/**
 * @brief Get stop bits
 *
 * @param port uart number
 * @return uart_stop_bits_t
 */
uart_stop_bits_t serial_get_stop_bits(uart_port_t port);

/**
 * @brief Get parity
 *
 * @param port uart number
 * @return uart_parity_t
 */
uart_parity_t serial_get_parity(uart_port_t port);

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

esp_err_t serial_set_config(uart_port_t port, const char* mode, int baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bits, uart_parity_t parity);

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
