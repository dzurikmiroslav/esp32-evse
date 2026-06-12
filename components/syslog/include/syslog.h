#ifndef SYSLOG_H_
#define SYSLOG_H_

#include <esp_err.h>
#include <stdbool.h>

#define SYSLOG_HOST_SIZE 64

/**
 * @brief Initialize remote syslog: open NVS, create queue + sender task,
 *        register the logger line callback. Call after network_init().
 */
void syslog_init(void);

/**
 * @brief Whether remote syslog forwarding is enabled.
 */
bool syslog_is_enabled(void);

/**
 * @brief Get configured syslog server host into a SYSLOG_HOST_SIZE buffer.
 *
 * @param value buffer of at least SYSLOG_HOST_SIZE bytes
 */
void syslog_get_host(char* value);

/**
 * @brief Set and persist syslog config. Applies live (no reboot).
 *
 * @param enabled enable/disable forwarding
 * @param host    server host/IP (may be NULL to leave unchanged)
 * @return ESP_OK or ESP_ERR_INVALID_ARG if host too long
 */
esp_err_t syslog_set_config(bool enabled, const char* host);

#endif /* SYSLOG_H_ */
