#include <stdint.h>

#define MODBUS_PACKET_SIZE                      256

#define MODBUS_READ_UINT16(buf, offset)         ((uint16_t)(buf[offset] << 8 | buf[offset + 1]))
#define MODBUS_WRITE_UINT16(buf, offset, value) \
    buf[offset] = value >> 8;                   \
    buf[offset + 1] = value & 0xFF;             \

typedef struct {
    uint16_t len;
    uint8_t data[MODBUS_PACKET_SIZE];
} modbus_packet_t;