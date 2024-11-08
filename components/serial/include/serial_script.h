#ifndef SERIAL_SCRIPT_H_
#define SERIAL_SCRIPT_H_

#include <driver/uart.h>

/**
 * @brief Start serial script api on uart
 *
 * @param uart_num
 * @param baud_rate
 * @param data_bits
 * @param stop_bit
 * @param parity
 * @param rs485z
 */
void serial_script_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485);

/**
 * @brief Stop serial script api
 *
 */
void serial_script_stop(void);

bool serial_script_is_available(void);

esp_err_t serial_script_write(const char *buf, size_t len);

esp_err_t serial_script_read(char* buf, size_t *len, uint32_t timeout);

#endif /* SERIAL_SCRIPT_H_ */
