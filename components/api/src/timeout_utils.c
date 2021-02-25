#include <string.h>
#include "esp_system.h"
#include "esp_timer.h"

#include "timeout_utils.h"
#include "wifi.h"

static void restart_timer_callback(void *arg)
{
    esp_restart();
}

void timeout_restart()
{
// @formatter:off
    const esp_timer_create_args_t timer_args = {
            .callback = &restart_timer_callback,
            .name = "restart"
    };
// @formatter:on
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_once(timer, 1000000);
}

typedef struct
{
    bool enabled;
    bool ssid_blank;
    char ssid[32];
    bool password_blank;
    char password[64];
} wifi_set_config_timer_arg_t;

static void wifi_set_config_timer_callback(void *arg)
{
    wifi_set_config_timer_arg_t *config = (wifi_set_config_timer_arg_t*) arg;
    wifi_set_config(config->enabled, config->ssid_blank ? NULL : config->ssid, config->password_blank ? NULL : config->password);
    free((void*) config);
}

void timeout_set_wifi_config(bool enabled, const char *ssid, const char *password)
{
    wifi_set_config_timer_arg_t *config = (wifi_set_config_timer_arg_t*) malloc(sizeof(wifi_set_config_timer_arg_t));
    config->enabled = enabled;
    if (ssid == NULL) {
        config->ssid_blank = true;
    } else {
        config->ssid_blank = false;
        strcpy(config->ssid, ssid);
    }
    if (password == NULL) {
        config->password_blank = true;
    } else {
        config->password_blank = false;
        strcpy(config->password, password);
    }

// @formatter:off
    const esp_timer_create_args_t timer_args = {
            .callback = &wifi_set_config_timer_callback,
            .name = "wifi_set_config",
            .arg = config
    };
// @formatter:on
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_once(timer, 1000000);
}
