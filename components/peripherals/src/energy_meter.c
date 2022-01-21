#include <memory.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/timer.h"
#include "esp_adc_cal.h"
#include "nvs.h"

#include "energy_meter.h"
#include "evse.h"
#include "board_config.h"

#define NVS_NAMESPACE           "evse_emeter"
#define NVS_AC_VOLTAGE          "ac_voltage"
#define NVS_EXT_PULSE_AMOUNT    "ext_pluse_amt"

#define PILOT_PWM_TIMER         LEDC_TIMER_0
#define PILOT_PWM_CHANNEL       LEDC_CHANNEL_0
#define PILOT_PWM_SPEED_MODE    LEDC_HIGH_SPEED_MODE
#define PILOT_PWM_MAX_DUTY      1023
#define ZERO_FIX                5000
#define MEASURE_US              40000   //2 periods at 50Hz


static const char* TAG = "energy_meter";

static nvs_handle nvs;

static esp_adc_cal_characteristics_t sens_adc_char;

static uint16_t power = 0;

static bool has_session = false;

static TickType_t session_start_tick = 0;

static TickType_t session_end_tick = 0;

static uint32_t session_consumption = 0;

static float cur[3] = { 0, 0, 0 };

static float vlt[3] = { 0, 0, 0 };

static float cur_sens_zero[3] = { 1650, 1650, 1650 }; //Default zero, midpoint 3.3V

static float vlt_sens_zero[3] = { 1650, 1650, 1650 }; //Default zero, midpoint 3.3V

static int64_t prev_time = 0;

static uint16_t ac_voltage;

static uint16_t ext_pulse_amount;

static SemaphoreHandle_t ext_pulse_semhr;

static uint32_t get_detla_ms()
{
    int64_t now = esp_timer_get_time();
    uint32_t delta = now - prev_time;
    prev_time = now;
    return delta / 1000;
}

static void (*measure_fn)(void);

static void set_calc_power(float p)
{
    session_consumption += roundf((p * get_detla_ms()) / 1000.0f);
    power = roundf(p);
}

static void measure_none(void)
{
    vlt[0] = ac_voltage;
    cur[0] = evse_get_chaging_current();
    float va = vlt[0] * cur[0];
    if (board_config.energy_meter_three_phases) {
        vlt[1] = vlt[2] = vlt[0];
        cur[1] = cur[2] = cur[0];
        va *= 3;
    }

    set_calc_power(va);
}

static uint32_t read_adc(adc_channel_t channel)
{
    int adc_reading = adc1_get_raw(channel);
    return esp_adc_cal_raw_to_voltage(adc_reading, &sens_adc_char);
}

static void measure_single_phase_cur(void)
{
    float cur_sum = 0;
    uint16_t samples = 0;
    uint32_t sample_cur;
    float filtered_cur;
    for (int64_t start_time = esp_timer_get_time(); esp_timer_get_time() - start_time < MEASURE_US; samples++) {
        sample_cur = read_adc(board_config.energy_meter_l1_cur_adc_channel);

        cur_sens_zero[0] += (sample_cur - cur_sens_zero[0]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[0];
        cur_sum += filtered_cur * filtered_cur;
    }

    cur[0] = sqrt(cur_sum / samples) * board_config.energy_meter_cur_scale;
    ESP_LOGD(TAG, "Current %f (samples %d)", cur[0], samples);

    set_calc_power(vlt[0] * cur[0]);
}

static void measure_single_phase_cur_vlt(void)
{
    float cur_sum = 0;
    float vlt_sum = 0;
    uint32_t sample_cur;
    uint32_t sample_vlt;
    float filtered_cur;
    float filtered_vlt;
    uint16_t samples = 0;

    for (int64_t start_time = esp_timer_get_time(); esp_timer_get_time() - start_time < MEASURE_US; samples++) {
        sample_cur = read_adc(board_config.energy_meter_l1_cur_adc_channel);
        sample_vlt = read_adc(board_config.energy_meter_l1_vlt_adc_channel);

        cur_sens_zero[0] += (sample_cur - cur_sens_zero[0]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[0];
        cur_sum += filtered_cur * filtered_cur;

        vlt_sens_zero[0] += (sample_vlt - vlt_sens_zero[0]) / ZERO_FIX;
        filtered_vlt = sample_vlt - vlt_sens_zero[0];
        vlt_sum += filtered_vlt * filtered_vlt;
    }

    cur[0] = sqrt(cur_sum / samples) * board_config.energy_meter_cur_scale;
    ESP_LOGD(TAG, "Current %f (samples %d)", cur[0], samples);

    vlt[0] = sqrt(vlt_sum / samples) * board_config.energy_meter_vlt_scale;
    ESP_LOGD(TAG, "Voltage %f (samples %d)", vlt[0], samples);

    set_calc_power(cur[0] * vlt[0]);
}

static void measure_three_phase_cur(void)
{
    float cur_sum[3] = { 0, 0, 0 };
    uint32_t sample_cur;
    float filtered_cur;
    uint16_t samples = 0;
    for (int64_t start_time = esp_timer_get_time(); esp_timer_get_time() - start_time < MEASURE_US; samples++) {
        //L1
        sample_cur = read_adc(board_config.energy_meter_l1_cur_adc_channel);

        cur_sens_zero[0] += (sample_cur - cur_sens_zero[0]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[0];
        cur_sum[0] += filtered_cur * filtered_cur;

        //L2
        sample_cur = read_adc(board_config.energy_meter_l2_cur_adc_channel);

        cur_sens_zero[1] += (sample_cur - cur_sens_zero[1]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[1];
        cur_sum[1] += filtered_cur * filtered_cur;

        //L3
        sample_cur = read_adc(board_config.energy_meter_l3_cur_adc_channel);

        cur_sens_zero[2] += (sample_cur - cur_sens_zero[2]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[2];
        cur_sum[2] += filtered_cur * filtered_cur;
    }

    cur[0] = sqrt(cur_sum[0] / samples) * board_config.energy_meter_cur_scale;
    cur[1] = sqrt(cur_sum[1] / samples) * board_config.energy_meter_cur_scale;
    cur[2] = sqrt(cur_sum[2] / samples) * board_config.energy_meter_cur_scale;
    ESP_LOGD(TAG, "Currents %f %f %f (samples %d)", cur[0], cur[1], cur[2], samples);

    set_calc_power(vlt[0] * cur[0] + vlt[1] * cur[1] + vlt[2] * cur[2]);
}

static void measure_three_phase_cur_vlt(void)
{
    float cur_sum[3] = { 0, 0, 0 };
    float vlt_sum[3] = { 0, 0, 0 };
    uint32_t sample_cur;
    uint32_t sample_vlt;
    float filtered_cur;
    float filtered_vlt;
    uint16_t samples = 0;
    for (int64_t start_time = esp_timer_get_time(); esp_timer_get_time() - start_time < MEASURE_US; samples++) {
        //L1
        sample_cur = read_adc(board_config.energy_meter_l1_cur_adc_channel);
        sample_vlt = read_adc(board_config.energy_meter_l2_vlt_adc_channel);

        cur_sens_zero[0] += (sample_cur - cur_sens_zero[0]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[0];
        cur_sum[0] += filtered_cur * filtered_cur;

        vlt_sens_zero[0] += (sample_vlt - vlt_sens_zero[0]) / ZERO_FIX;
        filtered_vlt = sample_vlt - vlt_sens_zero[0];
        vlt_sum[0] += filtered_vlt * filtered_vlt;

        //L2
        sample_cur = read_adc(board_config.energy_meter_l2_cur_adc_channel);
        sample_vlt = read_adc(board_config.energy_meter_l2_vlt_adc_channel);

        cur_sens_zero[1] += (sample_cur - cur_sens_zero[1]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[1];
        cur_sum[1] += filtered_cur * filtered_cur;

        vlt_sens_zero[1] += (sample_vlt - vlt_sens_zero[1]) / ZERO_FIX;
        filtered_vlt = sample_vlt - vlt_sens_zero[1];
        vlt_sum[1] += filtered_vlt * filtered_vlt;

        //L3
        sample_cur = read_adc(board_config.energy_meter_l3_cur_adc_channel);
        sample_vlt = read_adc(board_config.energy_meter_l3_vlt_adc_channel);

        cur_sens_zero[2] += (sample_cur - cur_sens_zero[2]) / ZERO_FIX;
        filtered_cur = sample_cur - cur_sens_zero[2];
        cur_sum[2] += filtered_cur * filtered_cur;

        vlt_sens_zero[2] += (sample_vlt - vlt_sens_zero[2]) / ZERO_FIX;
        filtered_vlt = sample_vlt - vlt_sens_zero[1];
        vlt_sum[2] += filtered_vlt * filtered_vlt;
    }

    cur[0] = sqrt(cur_sum[0] / samples) * board_config.energy_meter_cur_scale;
    cur[1] = sqrt(cur_sum[1] / samples) * board_config.energy_meter_cur_scale;
    cur[2] = sqrt(cur_sum[2] / samples) * board_config.energy_meter_cur_scale;
    ESP_LOGD(TAG, "Currents %f %f %f (samples %d)", cur[0], cur[1], cur[2], samples);

    vlt[0] = sqrt(vlt_sum[0] / samples) * board_config.energy_meter_vlt_scale;
    vlt[1] = sqrt(vlt_sum[1] / samples) * board_config.energy_meter_vlt_scale;
    vlt[2] = sqrt(vlt_sum[2] / samples) * board_config.energy_meter_vlt_scale;
    ESP_LOGD(TAG, "Voltages %f %f %f (samples %d)", vlt[0], vlt[1], vlt[2], samples);

    set_calc_power(vlt[0] * cur[0] + vlt[1] * cur[1] + vlt[2] * cur[2]);
}

static void IRAM_ATTR ext_pulse_isr_handler(void* arg)
{
    BaseType_t higher_task_woken = pdFALSE;

    xSemaphoreGiveFromISR(ext_pulse_semhr, &higher_task_woken);

    if (higher_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void measure_ext_pulse(void)
{
    uint8_t count = 0;
    while (xSemaphoreTake(ext_pulse_semhr, 0)) {
        count++;
    }

    uint16_t delta_consumtion = count * ext_pulse_amount;
    session_consumption += delta_consumtion;

    if (delta_consumtion > 0) {
        power = (delta_consumtion / get_detla_ms()) * 1000.0f;
    }
}

void energy_meter_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    ac_voltage = energy_meter_get_ac_voltage();
    ext_pulse_amount = energy_meter_get_ext_pulse_amount();

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_NONE) {
        measure_fn = &measure_none;
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &sens_adc_char);
        vlt[0] = ac_voltage;

        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l1_cur_adc_channel, ADC_ATTEN_DB_11));
        if (board_config.energy_meter_three_phases) {
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l2_cur_adc_channel, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l3_cur_adc_channel, ADC_ATTEN_DB_11));
            vlt[1] = ac_voltage;
            vlt[2] = ac_voltage;
            measure_fn = &measure_three_phase_cur;
        } else {
            measure_fn = &measure_single_phase_cur;
        }
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR_VLT) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &sens_adc_char);

        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l1_cur_adc_channel, ADC_ATTEN_DB_11));
        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l1_vlt_adc_channel, ADC_ATTEN_DB_11));

        if (board_config.energy_meter_three_phases) {
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l2_cur_adc_channel, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l3_cur_adc_channel, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l2_vlt_adc_channel, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l3_vlt_adc_channel, ADC_ATTEN_DB_11));
            measure_fn = &measure_three_phase_cur_vlt;
        } else {
            measure_fn = &measure_single_phase_cur_vlt;
        }
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_EXT_PULSE) {
        ext_pulse_semhr = xSemaphoreCreateCounting(10, 0);

        gpio_pad_select_gpio(board_config.energy_meter_ext_pulse_gpio);
        gpio_set_direction(board_config.energy_meter_ext_pulse_gpio, GPIO_MODE_INPUT);
        gpio_set_pull_mode(board_config.energy_meter_ext_pulse_gpio, GPIO_PULLDOWN_ONLY);
        gpio_set_intr_type(board_config.energy_meter_ext_pulse_gpio, GPIO_INTR_POSEDGE);
        gpio_isr_handler_add(board_config.energy_meter_ext_pulse_gpio, ext_pulse_isr_handler, NULL);

        measure_fn = &measure_ext_pulse;
    }
}

void energy_meter_set_config(uint16_t _ext_pulse_amount, uint16_t _ac_voltage)
{
    nvs_set_u16(nvs, NVS_EXT_PULSE_AMOUNT, _ext_pulse_amount);
    nvs_set_u16(nvs, NVS_AC_VOLTAGE, _ac_voltage);

    nvs_commit(nvs);

    ac_voltage = _ac_voltage;
    ext_pulse_amount = _ext_pulse_amount;
}

uint16_t energy_meter_get_ext_pulse_amount(void)
{
    uint16_t value = 1000;
    nvs_get_u16(nvs, NVS_EXT_PULSE_AMOUNT, &value);
    return value;
}

uint16_t energy_meter_get_ac_voltage(void)
{
    uint16_t value = 250;
    nvs_get_u16(nvs, NVS_AC_VOLTAGE, &value);
    return value;
}

void energy_meter_process(void)
{
    evse_state_t state = evse_get_state();

    if (!has_session && evse_state_is_session(state)) {
        ESP_LOGI(TAG, "Start session");
        session_start_tick = xTaskGetTickCount();
        session_end_tick = 0;
        session_consumption = 0;
        prev_time = esp_timer_get_time();
        has_session = true;
    } else if (!evse_state_is_session(state) && has_session) {
        ESP_LOGI(TAG, "End session");
        session_end_tick = xTaskGetTickCount();
        has_session = false;
    }

    if (evse_state_is_charging(state)) {
        (*measure_fn)();
    } else {
        vlt[0] = vlt[1] = vlt[2] = 0;
        cur[0] = cur[1] = cur[2] = 0;
        power = 0;
    }
}

uint16_t energy_meter_get_power(void)
{
    return power;
}

uint32_t energy_meter_get_session_elapsed(void)
{
    TickType_t elapsed = 0;

    if (has_session) {
        elapsed = xTaskGetTickCount() - session_start_tick;
    } else {
        elapsed = session_end_tick - session_start_tick;
    }

    return pdTICKS_TO_MS(elapsed) / 1000;
}

uint32_t energy_meter_get_session_consumption(void)
{
    return session_consumption;
}

void energy_meter_get_voltage(float* voltage)
{
    memcpy(voltage, vlt, sizeof(vlt));
}

void energy_meter_get_current(float* current)
{
    memcpy(current, cur, sizeof(cur));
}
