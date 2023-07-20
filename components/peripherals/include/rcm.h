#ifndef RCM_H_
#define RCM_H_

#include <stdbool.h>

/**
 * @brief Initialize residual current monitor
 *
 */
void rcm_init(void);

/**
 * @brief Test residual current monitor
 *
 * @return true
 * @return false
 */
bool rcm_test(void);

/**
 * @brief Residual current monitor was detected leakage
 *
 * @return true
 * @return false
 */
bool rcm_is_triggered(void);

#endif /* RCM_H_ */
