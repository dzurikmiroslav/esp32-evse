#ifndef AUX_H_
#define AUX_H_

#include "esp_err.h"

/**
 * @brief Aux identifiers
 * 
 */
typedef enum
{
    AUX_ID_1,
    AUX_ID_2,
    AUX_ID_3,
    AUX_ID_MAX
} aux_id_t;


/**
 * @brief Aux modes
 * 
 */
typedef enum
{
    AUX_MODE_NONE,
    AUX_MODE_ENABLE_BUTTON,
    AUX_MODE_ENABLE_SWITCH,
    AUX_MODE_AUTHORIZE_BUTTON,
    AUX_MODE_MAX
} aux_mode_t;

/**
 * @brief Initialize aux
 * 
 */
void aux_init(void);

/**
 * @brief Main loop of aux
 * 
 */
void aux_process(void);

/**
 * @brief Get aux mode, stored in NVS
 * 
 * @param id 
 * @return aux_mode_t 
 */
aux_mode_t aux_get_mode(aux_id_t id);

/**
 * @brief Set aux mode, stored in NVS
 * 
 * @param id 
 * @param mode 
 * @return esp_err_t
 */
esp_err_t aux_set_mode(aux_id_t id, aux_mode_t mode);

/**
 * @brief Format to string
 * 
 * @param mode 
 * @return const char* 
 */
const char* aux_mode_to_str(aux_mode_t mode);

/**
 * @brief Parse from string
 * 
 * @param str 
 * @return aux_mode_t 
 */
aux_mode_t aux_str_to_mode(const char* str);

#endif /* AUX_H_ */
