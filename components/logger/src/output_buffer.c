#include "output_buffer.h"

#include <memory.h>

output_buffer_t* output_buffer_create(uint16_t size)
{
    output_buffer_t* buffer = (output_buffer_t*)malloc(sizeof(output_buffer_t));

    buffer->size = size;
    buffer->count = 0;
    buffer->data = (uint8_t*)malloc(sizeof(uint8_t) * size);
    buffer->append = buffer->data;

    return buffer;
}

void output_buffer_delete(output_buffer_t* buffer)
{
    free((void*)buffer->data);
    free((void*)buffer);
}

void output_buffer_append_buf(output_buffer_t* buffer, const char* str, uint16_t len)
{
    if (((buffer->append - buffer->data) + sizeof(uint16_t) + len) >= buffer->size) {
        // rotate buffer
        uint8_t* pos = buffer->data;
        uint16_t rotate_count = 0;
        while ((pos - buffer->data) < buffer->size / 2) {
            // seek first half
            uint16_t entry_len;
            memcpy((void*)&entry_len, (void*)pos, sizeof(uint16_t));
            pos += entry_len + sizeof(uint16_t);
            rotate_count++;
        }

        memmove((void*)buffer->data, (void*)pos, buffer->size - (pos - buffer->data));
        buffer->count -= rotate_count;
        buffer->append -= (pos - buffer->data);
    }

    memcpy((void*)buffer->append, (void*)&len, sizeof(uint16_t));
    buffer->append += sizeof(uint16_t);

    memcpy((void*)buffer->append, (void*)str, len);
    buffer->append += len;

    buffer->count++;
}

void output_buffer_append_str(output_buffer_t* buffer, const char* str)
{
    output_buffer_append_buf(buffer, str, strlen(str));
}

bool output_buffer_read(output_buffer_t* buffer, uint16_t* index, char** str, uint16_t* len)
{
    if (*index > buffer->count) {
        *index = buffer->count;
    }

    bool has_next = false;

    if (*index < buffer->count) {
        uint8_t* pos = buffer->data;
        uint16_t current = 0;

        while (current != *index) {
            uint16_t entry_len;
            memcpy((void*)&entry_len, (void*)pos, sizeof(uint16_t));
            pos += entry_len + sizeof(uint16_t);
            current++;
        }

        memcpy((void*)len, (void*)pos, sizeof(uint16_t));
        pos += sizeof(uint16_t);
        *str = (char*)pos;

        (*index)++;

        has_next = true;
    }

    return has_next;
}