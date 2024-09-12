#include "temp_sensor.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/param.h>

#include "board_config.h"
#include "ds18x20.h"

#define MAX_SENSORS           5
#define MEASURE_PERIOD        10000  // 10s
#define MEASURE_ERR_THRESHOLD 3

static const char* TAG = "temp_sensor";

static ds18x20_addr_t sensor_addrs[MAX_SENSORS];

static uint8_t sensor_count = 0;

static int16_t low_temp = 0;

static int16_t high_temp = 0;

static uint8_t measure_err_count = 0;

static void temp_sensor_task_func(void* param)
{
    while (true) {
        int16_t temps[MAX_SENSORS];

        esp_err_t err = ds18x20_measure_and_read_multi(board_config.onewire_gpio, sensor_addrs, sensor_count, temps);
        if (err == ESP_OK) {
            int16_t low = INT16_MAX;
            int16_t high = INT16_MIN;

            for (int i = 0; i < sensor_count; i++) {
                low = MIN(low, temps[i]);
                high = MAX(high, temps[i]);
            }

            low_temp = low;
            high_temp = high;
            measure_err_count = 0;
        } else {
            ESP_LOGW(TAG, "Measure error %d (%s)", err, esp_err_to_name(err));
            measure_err_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(MEASURE_PERIOD));
    }
}

void temp_sensor_init(void)
{
    if (board_config.onewire && board_config.onewire_temp_sensor) {
        gpio_reset_pin(board_config.onewire_gpio);
        gpio_set_pull_mode(board_config.onewire_gpio, GPIO_PULLUP_ONLY);

        size_t found = 0;
        ESP_ERROR_CHECK(ds18x20_scan_devices(board_config.onewire_gpio, sensor_addrs, MAX_SENSORS, &found));

        if (found > MAX_SENSORS) {
            ESP_LOGW(TAG, "Found %d sensors, but can handle max %d", found, MAX_SENSORS);
        } else {
            ESP_LOGI(TAG, "Found %d sensors", found);
        }
        sensor_count = MIN(found, MAX_SENSORS);

        if (sensor_count > 0) {
            xTaskCreate(temp_sensor_task_func, "temp_sensor_task", 2 * 1024, NULL, 5, NULL);
        }
    }
}

uint8_t temp_sensor_get_count(void)
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

bool temp_sensor_is_error(void)
{
    return sensor_count == 0 || measure_err_count > MEASURE_ERR_THRESHOLD;
}