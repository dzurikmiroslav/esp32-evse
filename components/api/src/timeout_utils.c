#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "timeout_utils.h"
#include "wifi.h"

static void restart_fucn(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_restart();

    vTaskDelete(NULL);
}

void timeout_restart()
{
    xTaskCreate(restart_fucn, "restart_task", 2 * 1024, NULL, 10, NULL);
}

typedef struct
{
    bool enabled;
    bool ssid_blank;
    char ssid[32];
    bool password_blank;
    char password[64];
} wifi_set_config_arg_t;

static void wifi_set_config_func(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    wifi_set_config_arg_t *config = (wifi_set_config_arg_t*) arg;
    wifi_set_config(config->enabled, config->ssid_blank ? NULL : config->ssid, config->password_blank ? NULL : config->password);
    free((void*) config);

    vTaskDelete(NULL);
}

void timeout_set_wifi_config(bool enabled, const char *ssid, const char *password)
{
    wifi_set_config_arg_t *config = (wifi_set_config_arg_t*) malloc(sizeof(wifi_set_config_arg_t));
    config->enabled = enabled;
    if (ssid == NULL || ssid[0] == '\0') {
        config->ssid_blank = true;
    } else {
        config->ssid_blank = false;
        strcpy(config->ssid, ssid);
    }
    if (password == NULL || password[0] == '\0') {
        config->password_blank = true;
    } else {
        config->password_blank = false;
        strcpy(config->password, password);
    }

    xTaskCreate(wifi_set_config_func, "wifi_set_config", 4 * 1024, (void*) config, 10, NULL);
}
