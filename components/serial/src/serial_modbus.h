#ifndef SERIAL_MODBUS_H_
#define SERIAL_MODBUS_H_

#include <driver/uart.h>

/**
 * @brief Start serial Modbus
 *
 * @param uart_num
 * @param baud_rate
 * @param data_bits
 * @param stop_bit
 * @param parity
 * @param rs485
 */
void serial_modbus_start(uart_port_t uart_num, uint32_t baud_rate, uart_word_length_t data_bits, uart_stop_bits_t stop_bit, uart_parity_t parity, bool rs485);

/**
 * @brief Stop serial Modbus
 *
 */
void serial_modbus_stop(void);

#endif /* SERIAL_MODBUS_H_ */
