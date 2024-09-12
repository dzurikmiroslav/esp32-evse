#ifndef PROXIMITY_H_
#define PROXIMITY_H_

#include <stdint.h>

/**
 * @brief Initialize proximity check
 *
 */
void proximity_init(void);

/**
 * @brief Return measured value of max current on PP
 *
 * @return current in A
 */
uint8_t proximity_get_max_current(void);

#endif /* PROXIMITY_H_ */
