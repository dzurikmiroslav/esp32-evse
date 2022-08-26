#ifndef RCM_H_
#define RCM_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"

/**
 * @brief Output of residual current monitor
 *
 */
extern SemaphoreHandle_t rcm_semhr;

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
 * @brief Residual current monitor detected leakage
 *
 * @return true
 * @return false
 */
bool rcm_was_triggered(void);

#endif /* RCM_H_ */
