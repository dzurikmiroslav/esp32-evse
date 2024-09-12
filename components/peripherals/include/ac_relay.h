#ifndef AC_RELAY_H_
#define AC_RELAY_H_

#include <stdbool.h>

/**
 * @brief Initialize ac relay
 *
 */
void ac_relay_init(void);

/**
 * @brief Set state of ac relay
 *
 * @param state
 */
void ac_relay_set_state(bool state);

#endif /* AC_RELAY_H_ */
