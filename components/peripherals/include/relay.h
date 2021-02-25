#ifndef PERIPHERALS_H_
#define PERIPHERALS_H_

#include <stdbool.h>

//TODO relay enum

void ac_relay_init(void);

void ac_relay_set_state(bool state);

#endif /* PERIPHERALS_H_ */
