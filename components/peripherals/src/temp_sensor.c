#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "temp_sensor.h"
#include "board_config.h"
#include "onewire.h"

#define DS18X20_ANY                 ONEWIRE_NONE
#define DS18B20_FAMILY_ID           0x28
#define DS18S20_FAMILY_ID           0x10
#define DS18X20_WRITE_SCRATCHPAD    0x4E
#define DS18X20_READ_SCRATCHPAD     0xBE
#define DS18X20_COPY_SCRATCHPAD     0x48
#define DS18X20_READ_EEPROM         0xB8
#define DS18X20_READ_PWRSUPPLY      0xB4
#define DS18X20_SEARCHROM           0xF0
#define DS18X20_SKIP_ROM            0xCC
#define DS18X20_READROM             0x33
#define DS18X20_MATCHROM            0x55
#define DS18X20_ALARMSEARCH         0xEC
#define DS18X20_CONVERT_T           0x44

#define MAX_SENSORS                 5

#define RETURN_ON_ERROR(x) do {                 \
        esp_err_t err_rc_ = (x);                \
        if (unlikely(err_rc_ != ESP_OK)) {      \
            return err_rc_;                     \
        }                                       \
    } while(0)

static const char* TAG = "temp_sensor";

static onewire_addr_t sensor_addrs[MAX_SENSORS];

static uint8_t sensor_count = 0;

static SemaphoreHandle_t mutex;

void temp_sensor_init(void)
{
    mutex = xSemaphoreCreateMutex();

    if (board_config.temp_sensor) {
        onewire_search_t search;
        onewire_addr_t addr;
        onewire_search_start(&search);

        while ((addr = onewire_search_next(&search, board_config.temp_sensor_gpio)) != ONEWIRE_NONE) {
            ESP_LOGI(TAG, "Found sensor 0x%llu", addr);
            if (((uint8_t)addr) == DS18B20_FAMILY_ID || ((uint8_t)addr) == DS18S20_FAMILY_ID) {
                if (sensor_count == MAX_SENSORS) {
                    ESP_LOGW(TAG, "Max sensor counter reached");
                    break;
                }
                sensor_addrs[sensor_count++] = addr;
            }
        }
    }
}

uint8_t temp_sensor_count(void)
{
    return sensor_count;
}

static esp_err_t ds18x20_measure(void)
{
    if (!onewire_reset(board_config.temp_sensor_gpio)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    onewire_skip_rom(board_config.temp_sensor_gpio);

    onewire_write(board_config.temp_sensor_gpio, DS18X20_CONVERT_T);
    onewire_power(board_config.temp_sensor_gpio);

    return ESP_OK;
}

static esp_err_t ds18x20_read_scratchpad(onewire_addr_t addr, uint8_t* buffer)
{
    uint8_t crc;
    uint8_t expected_crc;

    if (!onewire_reset(board_config.temp_sensor_gpio)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (addr == DS18X20_ANY) {
        onewire_skip_rom(board_config.temp_sensor_gpio);
    } else {
        onewire_select(board_config.temp_sensor_gpio, addr);
    }
    onewire_write(board_config.temp_sensor_gpio, DS18X20_READ_SCRATCHPAD);

    for (int i = 0; i < 8; i++) {
        buffer[i] = onewire_read(board_config.temp_sensor_gpio);
    }

    crc = onewire_read(board_config.temp_sensor_gpio);

    expected_crc = onewire_crc8(buffer, 8);
    if (crc != expected_crc) {
        ESP_LOGE(TAG, "CRC check failed reading scratchpad: %02x %02x %02x %02x %02x %02x %02x %02x : %02x (expected %02x)",
            buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], crc, expected_crc);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

static esp_err_t ds18b20_read_temperature(onewire_addr_t addr, int16_t* temperature)
{
    uint8_t scratchpad[8];
    int16_t temp;

    RETURN_ON_ERROR(ds18x20_read_scratchpad(addr, scratchpad));

    temp = scratchpad[1] << 8 | scratchpad[0];

    *temperature = (temp * 625) / 100;

    return ESP_OK;
}

static esp_err_t ds18s20_read_temperature(onewire_addr_t addr, int16_t* temperature)
{
    uint8_t scratchpad[8];
    int16_t temp;

    RETURN_ON_ERROR(ds18x20_read_scratchpad(addr, scratchpad));

    temp = scratchpad[1] << 8 | scratchpad[0];

    temp = ((temp & 0xfffe) << 3) + (16 - scratchpad[6]) - 4;

    *temperature = (temp * 625) / 100 - 25;

    return ESP_OK;
}

static esp_err_t ds18x20_read_temperature(onewire_addr_t addr, int16_t* temperature)
{
    if (((uint8_t)addr) == DS18B20_FAMILY_ID) {
        return ds18b20_read_temperature(addr, temperature);
    } else {
        return ds18s20_read_temperature(addr, temperature);
    }
}

esp_err_t temp_sensor_measure(int16_t* low, int16_t* high)
{
    if (sensor_count == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    xSemaphoreTake(mutex, portMAX_DELAY);

    esp_err_t ret = ds18x20_measure();

    if (ret == ESP_OK) {
        *low = INT16_MAX;
        *high = INT16_MIN;

        for (int i = 0; i < sensor_count; i++) {
            int16_t temp;
            esp_err_t err = ds18x20_read_temperature(sensor_addrs[i], &temp);
            if (err != ESP_OK) {
                ret = err;
                break;
            }

            *low = MIN(*low, temp);
            *high = MAX(*high, temp);
        }
    }

    xSemaphoreGive(mutex);

    return ret;
}