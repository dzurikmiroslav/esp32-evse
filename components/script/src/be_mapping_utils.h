#ifndef __BE_MAPPING_UTILS__
#define __BE_MAPPING_UTILS__

#include "berry.h"

void be_map_insert_int(bvm *vm, const char *key, bint value);
void be_map_insert_bool(bvm *vm, const char *key, bbool value);
void be_map_insert_real(bvm *vm, const char *key, breal value);
void be_map_insert_str(bvm *vm, const char *key, const char *value);
void be_map_insert_list_uint8(bvm *vm, const char *key, const uint8_t *value, size_t size);

int be_map_bin_search(const char * needle, const void * table, size_t elt_size, size_t total_elements);

#endif // __BE_MAPPING_UTILS__
