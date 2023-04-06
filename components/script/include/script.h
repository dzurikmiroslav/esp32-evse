#ifndef SCRIPT_H_
#define SCRIPT_H_

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

#endif /* SCRIPT_H_ */
