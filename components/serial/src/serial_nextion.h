#ifndef SERIAL_NEXTION_H_
#define SERIAL_NEXTION_H_

#include <driver/uart.h>

/**
 * @brief Start serial Nextion
 *
 * @param uart_num
 * @param baud_rate
 * @param data_bits
 * @param stop_bit
 * @param parity
 * @param rs485
 */
void serial_nextion_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485);

/**
 * @brief Stop serial Nextion
 *
 */
void serial_nextion_stop(void);

#endif /* SERIAL_NEXTION_H_ */
