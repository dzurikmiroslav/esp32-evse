#ifndef SERIAL_LOGGER_H_
#define SERIAL_LOGGER_H_

#include <driver/uart.h>

/**
 * @brief Start serial logger on uart
 *
 * @param uart_num
 * @param baud_rate
 * @param data_bits
 * @param stop_bit
 * @param parity
 * @param rs485
 */
void serial_logger_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485);

/**
 * @brief Stop serial logger
 *
 */
void serial_logger_stop(void);

#endif /* SERIAL_LOGGER_H_ */
