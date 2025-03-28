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

/**
 * @brief If is Nextion available, send current time
 *
 */
void serial_nextion_set_time(void);

#define SERIAL_NEXTION_MODEL_MAX_SIZE 32

typedef struct {
    char model[SERIAL_NEXTION_MODEL_MAX_SIZE];
    bool touch : 1;
    uint32_t flash_size;
} serial_nextion_info_t;

esp_err_t serial_nextion_get_info(serial_nextion_info_t *info);

#define SERIAL_NEXTION_UPLOAD_BATCH_SIZE 4096

esp_err_t serial_nextion_upload_begin(size_t file_size, uint32_t baud_rate);

esp_err_t serial_nextion_upload_write(char *data, uint16_t len);

esp_err_t serial_nextion_upload_end(void);

#endif /* SERIAL_NEXTION_H_ */
