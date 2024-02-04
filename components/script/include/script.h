#ifndef SCRIPT_H_
#define SCRIPT_H_

#include "esp_err.h"
#include "cJSON.h"

/**
 * @brief Initialize script VM
 *
 */
void script_init(void);

/**
 * @brief Reload script VM
 *
 */
void script_reload(void);

/**
 * @brief Set enabled, stored in NVS
 *
 * @param enabled
 */
void script_set_enabled(bool enabled);

/**
 * @brief Get enabled, stored in NVS
 *
 * @return true
 * @return false
 */
bool script_is_enabled(void);

/**
 * @brief Get entries count
 *
 * @return uint16_t
 */
uint16_t script_output_count(void);

/**
 * @brief Read line from index, set index for reading next entry
 *
 * @param index
 * @param str
 * @param v
 * @return true When has next entry
 * @return false When no entry left
 */
bool script_output_read(uint16_t* index, char** str, uint16_t* len);

/**
 * @brief Get script drivers count
 *
 * @return uint8_t
 */
uint8_t script_driver_get_count(void);

typedef enum {
    SCRIPT_DRIVER_CONFIG_TYPE_STRING,
    SCRIPT_DRIVER_CONFIG_TYPE_NUMBER,
    SCRIPT_DRIVER_CONFIG_TYPE_BOOLEAN
}script_driver_config_type_t;

typedef struct {
    const char* key;
    script_driver_config_type_t type;
    union {
        const char* string;
        double number;
        bool boolean;
    } value;
} script_driver_config_value_meta_t;

typedef struct {
    const char* name;
    uint8_t config_entries_count;
    script_driver_config_value_meta_t* config_contries;
} script_driver_t;

/**
 * @brief Get script driver config json
 *
 * @param index
 * @return cJSON*
 */
cJSON* script_driver_read_config(uint8_t index);

script_driver_t* script_driver_get(uint8_t index);

/**
 * @brief Set script driver config json
 *
 * @param index
 * @param json
 */
void script_driver_write_config(uint8_t index, cJSON* json);

#endif /* SCRIPT_H_ */
