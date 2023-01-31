#include <memory.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "nvs.h"

#include "energy_meter.h"
#include "evse.h"
#include "board_config.h"
#include "aux_io.h"
#include "adc.h"

#define NVS_NAMESPACE           "evse_emeter"
#define NVS_MODE                "mode"
#define NVS_AC_VOLTAGE          "ac_voltage"
#define NVS_PULSE_AMOUNT        "pluse_amount"

#define ZERO_FIX                5000
#define MEASURE_US              40000   //2 periods at 50Hz


static const char* TAG = "energy_meter";

static nvs_handle nvs;

static energy_meter_mode_t mode = ENERGY_METER_MODE_NONE;

static uint16_t ac_voltage = 250;

static uint16_t pulse_amount = 1000;

static uint16_t power = 0;

static bool has_session = false;

static int64_t session_start_time = 0;

static int64_t session_end_time = 0;

static uint32_t session_consumption = 0;

static float cur[3] = { 0, 0, 0 };

static float vlt[3] = { 0, 0, 0 };

static float cur_sens_zero[3] = { 0, 0, 0 };

static float vlt_sens_zero[3] = { 0, 0, 0 };

static int64_t prev_time = 0;

static uint32_t delta_ms = 0;

static void (*measure_fn)(void);

static void set_calc_power(float p)
{
    session_consumption += roundf((p * delta_ms) / 1000.0f);
    power = roundf(p);
}

static void measure_none(void)
{
    vlt[0] = ac_voltage;
    cur[0] = evse_get_charging_current() / 10.0f;
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
    int adc_reading = 0;
    adc_oneshot_read(adc_handle, channel, &adc_reading);
    adc_cali_raw_to_voltage(adc_cali_handle, adc_reading, &adc_reading);
    return adc_reading;
}

static float get_zero(adc_channel_t channel)
{
    float sum = 0;
    uint16_t samples = 0;

    for (int64_t start_time = esp_timer_get_time(); esp_timer_get_time() - start_time < MEASURE_US; samples++) {
        sum += read_adc(channel);
    }

    return sum / samples;
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

    vlt[0] = ac_voltage;
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

    vlt[0] = vlt[1] = vlt[2] = ac_voltage;
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
        sample_vlt = read_adc(board_config.energy_meter_l1_vlt_adc_channel);

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
        filtered_vlt = sample_vlt - vlt_sens_zero[2];
        vlt_sum[2] += filtered_vlt * filtered_vlt;
    }

    cur[0] = sqrt(cur_sum[0] / samples) * board_config.energy_meter_cur_scale;
    cur[1] = sqrt(cur_sum[1] / samples) * board_config.energy_meter_cur_scale;
    cur[2] = sqrt(cur_sum[2] / samples) * board_config.energy_meter_cur_scale;
    ESP_LOGD(TAG, "Currents %fA %fA %fA (samples %d)", cur[0], cur[1], cur[2], samples);

    vlt[0] = sqrt(vlt_sum[0] / samples) * board_config.energy_meter_vlt_scale;
    vlt[1] = sqrt(vlt_sum[1] / samples) * board_config.energy_meter_vlt_scale;
    vlt[2] = sqrt(vlt_sum[2] / samples) * board_config.energy_meter_vlt_scale;
    ESP_LOGD(TAG, "Voltages %fV %fV %fV (samples %d)", vlt[0], vlt[1], vlt[2], samples);

    set_calc_power(vlt[0] * cur[0] + vlt[1] * cur[1] + vlt[2] * cur[2]);
}

static void measure_pulse(void)
{
    uint8_t count = 0;
    while (xSemaphoreTake(aux_pulse_energy_meter_semhr, 0)) {
        count++;
    }

    uint16_t delta_consumption = count * pulse_amount;
    session_consumption += delta_consumption;

    if (delta_consumption > 0) {
        power = (delta_consumption / delta_ms) * 1000.0f;
    }
}

static void* get_measure_fn(energy_meter_mode_t mode)
{
    switch (mode) {
    case ENERGY_METER_MODE_CUR:
        return board_config.energy_meter_three_phases ? measure_three_phase_cur : measure_single_phase_cur;
    case ENERGY_METER_MODE_CUR_VLT:
        return board_config.energy_meter_three_phases ? measure_three_phase_cur_vlt : measure_single_phase_cur_vlt;
    case ENERGY_METER_MODE_PULSE:
        return measure_pulse;
    default:
        return measure_none;
    }
}

void energy_meter_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    uint8_t u8 = ENERGY_METER_MODE_NONE;
    nvs_get_u8(nvs, NVS_MODE, &u8);
    mode = u8;
    measure_fn = get_measure_fn(mode);

    nvs_get_u16(nvs, NVS_AC_VOLTAGE, &ac_voltage);
    nvs_get_u16(nvs, NVS_PULSE_AMOUNT, &pulse_amount);
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11
    };

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR) {
        vlt[0] = ac_voltage;

        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l1_cur_adc_channel, &config));
        if (board_config.energy_meter_three_phases) {
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l2_cur_adc_channel, &config));
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l3_cur_adc_channel, &config));
        }
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR_VLT) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l1_cur_adc_channel, &config));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l1_vlt_adc_channel, &config));

        if (board_config.energy_meter_three_phases) {
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l2_cur_adc_channel, &config));
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l3_cur_adc_channel, &config));
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l2_vlt_adc_channel, &config));
            ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, board_config.energy_meter_l3_vlt_adc_channel, &config));
        }
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR || board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR_VLT) {
        cur_sens_zero[0] = get_zero(board_config.energy_meter_l1_cur_adc_channel);
        if (board_config.energy_meter_three_phases) {
            cur_sens_zero[1] = get_zero(board_config.energy_meter_l2_cur_adc_channel);
            cur_sens_zero[2] = get_zero(board_config.energy_meter_l3_cur_adc_channel);
        }
        ESP_LOGI(TAG, "Current zero %f %f %f", cur_sens_zero[0], cur_sens_zero[1], cur_sens_zero[2]);

        if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR_VLT) {
            vlt_sens_zero[0] = get_zero(board_config.energy_meter_l1_vlt_adc_channel);
            if (board_config.energy_meter_three_phases) {
                vlt_sens_zero[1] = get_zero(board_config.energy_meter_l2_vlt_adc_channel);
                vlt_sens_zero[2] = get_zero(board_config.energy_meter_l3_vlt_adc_channel);
            }
            ESP_LOGI(TAG, "Voltage zero %f %f %f", vlt_sens_zero[0], vlt_sens_zero[1], vlt_sens_zero[2]);
        }
    }
}

energy_meter_mode_t energy_meter_get_mode(void)
{
    return mode;
}

esp_err_t energy_meter_set_mode(energy_meter_mode_t _mode)
{
    if (_mode < 0 || _mode >= ENERGY_METER_MODE_MAX) {
        ESP_LOGE(TAG, "Mode out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_NONE) {
        if (_mode == ENERGY_METER_MODE_CUR || _mode == ENERGY_METER_MODE_CUR_VLT) {
            ESP_LOGE(TAG, "Unsupported mode");
            return ESP_ERR_NOT_SUPPORTED;
        }
    }

    if (board_config.energy_meter == BOARD_CONFIG_ENERGY_METER_CUR) {
        if (_mode == ENERGY_METER_MODE_CUR_VLT) {
            ESP_LOGE(TAG, "Unsupported mode");
            return ESP_ERR_NOT_SUPPORTED;
        }
    }

    mode = _mode;
    measure_fn = get_measure_fn(mode);
    nvs_set_u8(nvs, NVS_MODE, mode);
    nvs_commit(nvs);

    return ESP_OK;
}

uint16_t energy_meter_get_pulse_amount(void)
{
    return pulse_amount;
}

esp_err_t energy_meter_set_pulse_amount(uint16_t _pulse_amount)
{
    if (_pulse_amount == 0) {
        ESP_LOGI(TAG, "Pulse amount cant be zero");
        return ESP_ERR_INVALID_ARG;
    }

    pulse_amount = _pulse_amount;
    nvs_set_u16(nvs, NVS_PULSE_AMOUNT, pulse_amount);
    nvs_commit(nvs);

    return ESP_OK;
}

uint16_t energy_meter_get_ac_voltage(void)
{
    return ac_voltage;
}

esp_err_t energy_meter_set_ac_voltage(uint16_t _ac_voltage)
{
    if (_ac_voltage < 100 || _ac_voltage > 300) {
        ESP_LOGI(TAG, "AC voltage out of range");
        return ESP_ERR_INVALID_ARG;
    }

    ac_voltage = _ac_voltage;
    nvs_set_u16(nvs, NVS_AC_VOLTAGE, ac_voltage);
    nvs_commit(nvs);

    return ESP_OK;
}

void energy_meter_process(void)
{
    evse_state_t state = evse_get_state();
    int64_t now = esp_timer_get_time();

    if (!has_session && evse_state_is_session(state)) {
        ESP_LOGI(TAG, "Start session");
        session_start_time = now;
        session_end_time = 0;
        session_consumption = 0;
        has_session = true;
    } else if (!evse_state_is_session(state) && has_session) {
        ESP_LOGI(TAG, "End session");
        session_end_time = now;
        has_session = false;
    }

    delta_ms = (now - prev_time) / 1000;

    if (evse_state_is_charging(state)) {
        (*measure_fn)();
    } else {
        vlt[0] = vlt[1] = vlt[2] = 0;
        cur[0] = cur[1] = cur[2] = 0;
        power = 0;
    }

    prev_time = now;
}

uint16_t energy_meter_get_power(void)
{
    return power;
}

uint32_t energy_meter_get_session_elapsed(void)
{
    int64_t elapsed = 0;

    if (has_session) {
        elapsed = esp_timer_get_time() - session_start_time;
    } else {
        elapsed = session_end_time - session_start_time;
    }

    return elapsed / 1000000;
}

uint32_t energy_meter_get_session_consumption(void)
{
    return session_consumption;
}

void energy_meter_get_voltage(float* voltage)
{
    memcpy(voltage, vlt, sizeof(vlt));
}

float energy_meter_get_l1_voltage(void)
{
    return vlt[0];
}

float energy_meter_get_l2_voltage(void)
{
    return vlt[1];
}

float energy_meter_get_l3_voltage(void)
{
    return vlt[2];
}

void energy_meter_get_current(float* current)
{
    memcpy(current, cur, sizeof(cur));
}

float energy_meter_get_l1_current(void)
{
    return cur[0];
}

float energy_meter_get_l2_current(void)
{
    return cur[2];
}

float energy_meter_get_l3_current(void)
{
    return cur[3];
}

const char* energy_meter_mode_to_str(energy_meter_mode_t mode)
{
    switch (mode)
    {
    case ENERGY_METER_MODE_CUR:
        return "cur";
    case ENERGY_METER_MODE_CUR_VLT:
        return "cur_vlt";
    case ENERGY_METER_MODE_PULSE:
        return "pulse";
    default:
        return "none";
    }
}

energy_meter_mode_t energy_meter_str_to_mode(const char* str)
{
    if (!strcmp(str, "cur")) {
        return ENERGY_METER_MODE_CUR;
    }
    if (!strcmp(str, "cur_vlt")) {
        return ENERGY_METER_MODE_CUR_VLT;
    }
    if (!strcmp(str, "pulse")) {
        return ENERGY_METER_MODE_PULSE;
    }
    return ENERGY_METER_MODE_NONE;
}