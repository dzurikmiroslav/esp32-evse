#ifndef SCRIPT_H_
#define SCRIPT_H_

#include <esp_err.h>
#include <stdbool.h>
#include <sys/queue.h>

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
 * @brief Set auto reload, stored in NVS
 *
 * @param auto_reload
 */
void script_set_auto_reload(bool auto_reload);

/**
 * @brief Get auto reload, stored in NVS
 *
 * @return true
 * @return false
 */
bool script_is_auto_reload(void);

/**
 * @brief Notify script file was changed
 *
 * @param path
 */
void script_file_changed(const char* path);

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

/**
 * @brief Script component param type
 *
 */
typedef enum {
    SCRIPT_COMPONENT_PARAM_TYPE_NONE,
    SCRIPT_COMPONENT_PARAM_TYPE_STRING,
    SCRIPT_COMPONENT_PARAM_TYPE_NUMBER,
    SCRIPT_COMPONENT_PARAM_TYPE_BOOLEAN
} script_component_param_type_t;

/**
 * @brief Parse from string
 *
 * @param str
 * @return script_component_param_type_t
 */
script_component_param_type_t script_str_to_component_param_type(const char* str);

/**
 * @brief Serialize to string
 *
 * @param type
 * @return const char*
 */
const char* script_component_param_type_to_str(script_component_param_type_t type);

/**
 * @brief Script component parameter
 *
 */
typedef struct script_component_param_entry_s {
    char* key;
    char* name;
    script_component_param_type_t type;

    union {
        char* string;
        double number;
        bool boolean;
    } value;

    SLIST_ENTRY(script_component_param_entry_s) entries;
} script_component_param_entry_t;

/**
 * @brief Script component parameter list
 *
 * @return typedef
 */
typedef SLIST_HEAD(script_component_param_list_s, script_component_param_entry_s) script_component_param_list_t;

/**
 * @brief Get component parameters list, stored in file
 *
 * @param id Component id
 * @return script_component_param_list_t*
 */
script_component_param_list_t* script_get_component_params(const char* id);

/**
 * @brief Set component parameters list, stored in file
 *
 * @param id Component id
 * @param list
 */
void script_set_component_params(const char* id, script_component_param_list_t* list);

/**
 * @brief Free component parameters list with all entries
 *
 * @param list
 */
void script_component_params_free(script_component_param_list_t* list);

/**
 * @brief Script component
 *
 */
typedef struct script_component_entry_s {
    char* id;
    char* name;
    char* description;

    SLIST_ENTRY(script_component_entry_s) entries;
} script_component_entry_t;

/**
 * @brief Script components list
 *
 * @return typedef
 */
typedef SLIST_HEAD(script_component_list_s, script_component_entry_s) script_component_list_t;

/**
 * @brief Get script components list
 *
 * @return script_component_list_t*
 */
script_component_list_t* script_get_components(void);

/**
 * @brief Free script components list with all entries
 *
 * @param list
 */
void script_components_free(script_component_list_t* list);

#endif /* SCRIPT_H_ */
