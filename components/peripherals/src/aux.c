#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs.h"

#include "aux.h"
#include "board_config.h"

#define NVS_NAMESPACE           "aux"
#define NVS_TYPE                "type_%x"

static const char* TAG = "aux";

static nvs_handle_t nvs;

static SemaphoreHandle_t mutex;

SemaphoreHandle_t aux_pulse_energy_meter_semhr;

static struct aux_s
{
    gpio_num_t gpio;
    aux_mode_t mode;
} auxs[3];

void aux_init(void)
{
    mutex = xSemaphoreCreateMutex();
    aux_pulse_energy_meter_semhr = xSemaphoreCreateCounting(10, 0);

    for (int i = 0; i < AUX_ID_MAX; i++) {
        auxs[i].gpio = GPIO_NUM_NC;
        auxs[i].mode = AUX_MODE_NONE;
    }

    gpio_config_t io_conf =  {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = 0
    };

    if (board_config.aux_1) {
        auxs[AUX_ID_1].gpio = board_config.aux_1_gpio;
        io_conf.pin_bit_mask |= 1ULL << board_config.aux_1_gpio;
    }

    if (board_config.aux_2) {
        auxs[AUX_ID_2].gpio = board_config.aux_2_gpio;
        io_conf.pin_bit_mask |= 1ULL << board_config.aux_2_gpio;
    }

    if (board_config.aux_3) {
        auxs[AUX_ID_3].gpio = board_config.aux_3_gpio;
        io_conf.pin_bit_mask |= 1ULL << board_config.aux_3_gpio;
    }

    if (io_conf.pin_bit_mask > 0) {
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    for (int i = 0; i < AUX_ID_MAX; i++) {
        if (auxs[i].gpio != GPIO_NUM_NC) {
            uint8_t u8 = 0;
            char key[12];
            sprintf(key, NVS_TYPE, i);
            nvs_get_u8(nvs, key, &u8);
            auxs[i].mode = u8;
        }
    }
}

void aux_process(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    xSemaphoreGive(mutex);
}

aux_mode_t aux_get_mode(aux_id_t id)
{
    return auxs[id].mode;
}

void aux_set_mode(aux_id_t id, aux_mode_t mode)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (auxs[id].gpio == GPIO_NUM_NC) {
        if (mode != AUX_MODE_NONE) {
            ESP_LOGW(TAG, "Try configure not available input %d", id);
        }
    } else {
        auxs[id].mode = mode;

        char key[12];
        sprintf(key, NVS_TYPE, id);
        nvs_set_u8(nvs, key, mode);
        nvs_commit(nvs);
    }

    xSemaphoreGive(mutex);
}