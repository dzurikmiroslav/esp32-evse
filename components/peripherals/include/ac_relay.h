#ifndef AC_RELAY_H_
#define AC_RELAY_H_

#include <stdbool.h>

//TODO relay enum

void ac_relay_init(void);

void ac_relay_set_state(bool state);

#endif /* AC_RELAY_H_ */
