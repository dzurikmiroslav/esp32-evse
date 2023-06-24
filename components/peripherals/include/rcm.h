#ifndef RCM_H_
#define RCM_H_

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
void rcm_test(void);

#endif /* RCM_H_ */
