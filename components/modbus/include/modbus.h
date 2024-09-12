#ifndef MODBUS_H_
#define MODBUS_H_

#include <esp_err.h>

#define MODBUS_PACKET_SIZE 256

#define MODBUS_READ_UINT16(buf, offset) ((uint16_t)(buf[offset] << 8 | buf[offset + 1]))
#define MODBUS_WRITE_UINT16(buf, offset, value) \
    buf[offset] = value >> 8;                   \
    buf[offset + 1] = value & 0xFF;

/**
 * @brief Initialize modbus
 *
 */
void modbus_init(void);

/**
 * @brief Process modbus request
 *
 * @param buf Request/response data
 * @param len Length of request data
 * @return uint16_t Length of response data, 0 if no response
 */
uint16_t modbus_request_exec(uint8_t *buf, uint16_t len);

/**
 * @brief Get modbus unit id
 *
 * @return uint8_t
 */
uint8_t modbus_get_unit_id(void);

/**
 * @brief Set modbus unit id
 *
 * @param unit_id
 */
esp_err_t modbus_set_unit_id(uint8_t unit_id);

#endif /* MODBUS_H_ */
