#ifndef TCP_LOGGER_H_
#define TCP_LOGGER_H_

#include <stdbool.h>
#include "esp_err.h"

void tcp_logger_init(void);

esp_err_t tcp_logger_set_config(bool enabled, uint16_t port);

bool tcp_logger_get_enabled(void);

uint16_t tcp_logger_get_port(void);

void tcp_logger_log_process(const char *str, int len);

#endif /* TCP_LOGGER_H_ */
