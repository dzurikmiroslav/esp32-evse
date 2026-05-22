#ifndef SERIAL_SCRIPT_H_
#define SERIAL_SCRIPT_H_

#include <esp_err.h>

bool serial_script_is_available(void);

esp_err_t serial_script_write(const char *buf, size_t len);

esp_err_t serial_script_read(char *buf, size_t *len);

esp_err_t serial_script_flush(void);

#endif /* SERIAL_SCRIPT_H_ */