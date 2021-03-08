#include <memory.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/timer.h"
#include "esp_adc_cal.h"

#include "energy_meter.h"
#include "evse.h"
#include "board_config.h"

#define PILOT_PWM_TIMER         LEDC_TIMER_0
#define PILOT_PWM_CHANNEL       LEDC_CHANNEL_0
#define PILOT_PWM_SPEED_MODE    LEDC_HIGH_SPEED_MODE
#define PILOT_PWM_MAX_DUTY      1023
#define ZERO_FIX                5000
#define MEASURE_US              40000   //2 periods at 50Hz
#define AC_VOLTAGE              230     //TODO from board config?

static const char *TAG = "energy_meter";

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

static uint32_t get_detla_ms()
{
    int64_t now = esp_timer_get_time();
    uint32_t delta = now - prev_time;
    prev_time = now;
    return delta / 1000;
}

static float (*calc_power_fn)(void);

float calc_power_none(void)
{
    float va = AC_VOLTAGE * evse_get_chaging_current();
    if (board_config.energy_meter_three_phases) {
        va *= 3;
    }
    return va;
}

uint32_t read_adc(adc_channel_t channel)
{
    int adc_reading = adc1_get_raw(board_config.energy_meter_l1_cur_adc_channel);
    return esp_adc_cal_raw_to_voltage(adc_reading, &sens_adc_char);
}

float calc_power_single_phase_cur(void)
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
    ESP_LOGI(TAG, "Current %f (samples %d)", cur[0], samples);

    return vlt[0] * cur[0];
}

float calc_power_single_phase_cur_vlt(void)
{
    float cur_sum = 0;
    float vlt_sum = 0;
    float power_sum = 0;
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

        power_sum += (filtered_vlt * board_config.energy_meter_vlt_scale) * (filtered_cur * board_config.energy_meter_cur_scale);
    }

    cur[0] = sqrt(cur_sum / samples) * board_config.energy_meter_cur_scale;
    ESP_LOGI(TAG, "Current %f (samples %d)", cur[0], samples);

    vlt[0] = sqrt(vlt_sum / samples) * board_config.energy_meter_vlt_scale;
    ESP_LOGI(TAG, "Voltage %f (samples %d)", vlt[0], samples);

    return power_sum / samples;
}

float calc_power_three_phase_cur(void)
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
    ESP_LOGI(TAG, "Currents %f %f %f (samples %d)", cur[0], cur[1], cur[2], samples);

    return vlt[0] * cur[0] + vlt[1] * cur[1] + vlt[2] * cur[2];
}

float calc_power_three_phase_cur_vlt(void)
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
    ESP_LOGI(TAG, "Currents %f %f %f (samples %d)", cur[0], cur[1], cur[2], samples);

    vlt[0] = sqrt(vlt_sum[0] / samples) * board_config.energy_meter_vlt_scale;
    vlt[1] = sqrt(vlt_sum[1] / samples) * board_config.energy_meter_vlt_scale;
    vlt[2] = sqrt(vlt_sum[2] / samples) * board_config.energy_meter_vlt_scale;
    ESP_LOGI(TAG, "Voltages %f %f %f (samples %d)", vlt[0], vlt[1], vlt[2], samples);

    return vlt[0] * cur[0] + vlt[1] * cur[1] + vlt[2] * cur[2];
}

float calc_power_ext_pulse(void)
{
    ESP_LOGW(TAG, "TODO process_ext_pulse");
    return 0;
}

void energy_meter_init(void)
{
    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_NONE) {
        calc_power_fn = &calc_power_none;
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &sens_adc_char);
        vlt[0] = 230; //set constant voltage, TODO may from board.cfg ??

        ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l1_cur_adc_channel, ADC_ATTEN_DB_11));
        if (board_config.energy_meter_three_phases) {
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l2_cur_adc_channel, ADC_ATTEN_DB_11));
            ESP_ERROR_CHECK(adc1_config_channel_atten(board_config.energy_meter_l3_cur_adc_channel, ADC_ATTEN_DB_11));
            vlt[1] = AC_VOLTAGE;
            vlt[2] = AC_VOLTAGE;
            calc_power_fn = &calc_power_three_phase_cur;
        } else {
            calc_power_fn = &calc_power_single_phase_cur;
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
            calc_power_fn = &calc_power_three_phase_cur_vlt;
        } else {
            calc_power_fn = &calc_power_single_phase_cur_vlt;
        }
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_EXT_PULSE) {
        calc_power_fn = &calc_power_ext_pulse;
    }
}

void energy_meter_process(void)
{
    evse_state_t state = evse_get_state();

    if (!has_session && evse_state_in_session(state)) {
        ESP_LOGI(TAG, "Start session");
        session_start_tick = xTaskGetTickCount();
        session_end_tick = 0;
        session_consumption = 0;
        prev_time = esp_timer_get_time();
        has_session = true;
    } else if (!evse_state_in_session(state) && has_session) {
        ESP_LOGI(TAG, "End session");
        session_end_tick = xTaskGetTickCount();
        has_session = false;
    }

    if (evse_state_relay_closed(state)) {
        power = roundf((*calc_power_fn)());
        session_consumption += roundf((power * get_detla_ms()) / 1000.0f);
    } else {
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

void energy_meter_get_voltage(float *voltage)
{
    memcpy(voltage, vlt, sizeof(vlt));
}

void energy_meter_get_current(float *current)
{
    memcpy(current, cur, sizeof(cur));
}
