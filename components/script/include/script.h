#ifndef SCRIPT_H_
#define SCRIPT_H_

#include <esp_err.h>
#include <stdbool.h>

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
    SCRIPT_DRIVER_CFG_ENTRY_TYPE_NONE,
    SCRIPT_DRIVER_CFG_ENTRY_TYPE_STRING,
    SCRIPT_DRIVER_CFG_ENTRY_TYPE_NUMBER,
    SCRIPT_DRIVER_CFG_ENTRY_TYPE_BOOLEAN
} script_driver_cfg_entry_type_t;

script_driver_cfg_entry_type_t script_str_to_driver_cfg_entry_type(const char* str);

const char* script_driver_cfg_entry_type_to_str(script_driver_cfg_entry_type_t type);

typedef struct {
    char* key;
    char* name;
    script_driver_cfg_entry_type_t type;

    union {
        char* string;
        double number;
        bool boolean;
    } value;
} script_driver_cfg_meta_entry_t;

typedef struct {
    char* name;
    char* description;
    uint8_t cfg_entries_count;
    script_driver_cfg_meta_entry_t* cfg_entries;
} script_driver_t;

typedef struct {
    char* key;
    script_driver_cfg_entry_type_t type;

    union {
        char* string;
        double number;
        bool boolean;
    } value;
} script_driver_cfg_entry_t;

script_driver_t* script_driver_get(uint8_t index);

void script_driver_free(script_driver_t* script_driver);

void script_driver_cfg_entries_free(script_driver_cfg_entry_t* cfg_entries, uint8_t cfg_entries_count);

void script_driver_set_config(uint8_t driver_index, const script_driver_cfg_entry_t* cfg_entries, uint8_t cfg_entries_count);

#endif /* SCRIPT_H_ */
