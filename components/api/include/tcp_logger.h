#ifndef TCP_LOGGER_H_
#define TCP_LOGGER_H_

#include <stdbool.h>

void tcp_logger_init(void);

void tcp_logger_set_config(bool enabled, uint16_t port);

bool tcp_logger_get_enabled(void);

uint16_t tcp_logger_get_port(void);

#endif /* TCP_LOGGER_H_ */
