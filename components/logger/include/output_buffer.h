#ifndef OUTPUT_BUFFER_H_
#define OUTPUT_BUFFER_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t size;
    uint16_t count;
    uint8_t* data;
    uint8_t* append;
    bool is_static : 1;
} output_buffer_t;

output_buffer_t* output_buffer_create(uint16_t size);

output_buffer_t* output_buffer_create_static(uint16_t size, uint8_t* buf);

void output_buffer_delete(output_buffer_t* buffer);

void output_buffer_append_buf(output_buffer_t* buffer, const char* buf, uint16_t len);

void output_buffer_append_str(output_buffer_t* buffer, const char* str);

bool output_buffer_read(output_buffer_t* buffer, uint16_t* index, char** str, uint16_t* len);

#endif /* OUTPUT_BUFFER_H_ */