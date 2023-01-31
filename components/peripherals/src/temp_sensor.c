#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "temp_sensor.h"
#include "board_config.h"
#include "ds18x20.h"

#define MAX_SENSORS                 5
#define MEASURE_PERIOD              10000 //10s

static const char* TAG = "temp_sensor";

static ds18x20_addr_t sensor_addrs[MAX_SENSORS];

static uint8_t sensor_count = 0;

static int16_t low_temp = 0;

static int16_t high_temp = 0;

static esp_err_t error = ESP_OK;

static void temp_sensor_task_func(void* param)
{
    while (true) {
        int16_t temps[MAX_SENSORS];

        error = ds18x20_measure_and_read_multi(board_config.temp_sensor_gpio, sensor_addrs, sensor_count, temps);
        if (error == ESP_OK) {
            int16_t low = INT16_MAX;
            int16_t high = INT16_MIN;

            for (int i = 0; i < sensor_count; i++) {
                low = MIN(low, temps[i]);
                high = MAX(high, temps[i]);
            }

            low_temp = low;
            high_temp = high;
        }
        vTaskDelay(pdMS_TO_TICKS(MEASURE_PERIOD));
    }
}

void temp_sensor_init(void)
{
    if (board_config.temp_sensor) {
        gpio_reset_pin(board_config.temp_sensor_gpio);

        size_t found = 0;
        ESP_ERROR_CHECK(ds18x20_scan_devices(board_config.temp_sensor_gpio, sensor_addrs, MAX_SENSORS, &found));

        if (found == 0) {
            error = ESP_ERR_NOT_FOUND;
        } else if (found > MAX_SENSORS) {
            ESP_LOGW(TAG, "Found %d sensors, but cant handle max %d", found, MAX_SENSORS);
        } else {
            ESP_LOGI(TAG, "Found %d sensors", found);
        }
        sensor_count = MIN(found, MAX_SENSORS);

        if (sensor_count > 0) {
            xTaskCreate(temp_sensor_task_func, "temp_sensor_task", 1 * 1024, NULL, 5, NULL);
        }
    }
}

uint8_t temp_sensor_count(void)
{
    return sensor_count;
}

int16_t temp_sensor_get_low(void)
{
    return low_temp;
}

int16_t temp_sensor_get_high(void)
{
    return high_temp;
}

esp_err_t temp_sensor_get_error(void)
{
    return error;
}