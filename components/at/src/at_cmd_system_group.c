#include <esp_chip_info.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <string.h>
#include <sys/time.h>

#include "cat.h"
#include "temp_sensor.h"
#include "vars.h"

static void restart_func(void* arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
    vTaskDelete(NULL);
}

static void timeout_restart(void)
{
    xTaskCreate(restart_func, "restart_task", 2 * 1024, NULL, 10, NULL);
}

static cat_return_state cmd_rst_run(const struct cat_command* cmd)
{
    timeout_restart();

    return CAT_RETURN_STATE_OK;
}

static int vars_chip_chip_cores_revision_read(const struct cat_variable* var)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    strcpy(var_str32_1, CONFIG_IDF_TARGET);
    var_u8_1 = chip_info.cores;
    var_u16_1 = chip_info.revision;

    return 0;
}

static struct cat_variable vars_chip[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = vars_chip_chip_cores_revision_read,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u16_1,
        .data_size = sizeof(var_u16_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static int vars_heap_read(const struct cat_variable* var)
{
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL);

    var_u32_1 = heap_info.total_allocated_bytes;
    var_u32_2 = heap_info.total_free_bytes + heap_info.total_allocated_bytes;

    return 0;
}

static struct cat_variable vars_heap[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u32_1,
        .data_size = sizeof(var_u32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = vars_heap_read,
    },
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u32_2,
        .data_size = sizeof(var_u32_2),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },

};

static int var_ver_read(const struct cat_variable* var)
{
    const esp_app_desc_t* app_desc = esp_app_get_description();

    strcpy(var_str32_1, app_desc->version);

    return 0;
}

static struct cat_variable vars_ver[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_ver_read,
    },
};

static int var_idf_ver_read(const struct cat_variable* var)
{
    const esp_app_desc_t* app_desc = esp_app_get_description();

    strcpy(var_str32_1, app_desc->idf_ver);

    return 0;
}

static struct cat_variable vars_idf_ver[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_idf_ver_read,
    },
};

static int var_buildtime_date_time_read(const struct cat_variable* var)
{
    const esp_app_desc_t* app_desc = esp_app_get_description();

    strcpy(var_str32_1, app_desc->date);
    strcpy(var_str32_2, app_desc->time);

    return 0;
}

static struct cat_variable vars_buildtime[] = {
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_1,
        .data_size = sizeof(var_str32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = var_buildtime_date_time_read,
    },
    {
        .type = CAT_VAR_BUF_STRING,
        .data = var_str32_2,
        .data_size = sizeof(var_str32_2),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static int vars_temperature_read(const struct cat_variable* var)
{
    var_u8_1 = temp_sensor_get_count();
    var_i32_1 = temp_sensor_get_high();
    var_i32_2 = temp_sensor_get_low();

    return 0;
}

static struct cat_variable vars_temperature[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u8_1,
        .data_size = sizeof(var_u8_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
        .read = vars_temperature_read,
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_1,
        .data_size = sizeof(var_i32_1),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
    {
        .type = CAT_VAR_INT_DEC,
        .data = &var_i32_2,
        .data_size = sizeof(var_i32_2),
        .access = CAT_VAR_ACCESS_READ_ONLY,
    },
};

static int var_time_read(const struct cat_variable* var)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    var_u64_1 = tv.tv_sec;

    return 0;
}

static int var_time_write(const struct cat_variable* var, const size_t write_size)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tv.tv_sec = var_u64_1;
    settimeofday(&tv, NULL);

    return 0;
}

static struct cat_variable vars_time[] = {
    {
        .type = CAT_VAR_UINT_DEC,
        .data = &var_u64_1,
        .data_size = sizeof(var_u64_1),
        .access = CAT_VAR_ACCESS_READ_WRITE,
        .read = var_time_read,
        .write = var_time_write,
    },
};

static struct cat_command cmds[] = {
    {
        .name = "+RST",
        .run = cmd_rst_run,
    },
    {
        .name = "+CHIP",
        .var = vars_chip,
        .var_num = sizeof(vars_chip) / sizeof(vars_chip[0]),
    },
    {
        .name = "+HEAP",
        .var = vars_heap,
        .var_num = sizeof(vars_heap) / sizeof(vars_heap[0]),
    },
    {
        .name = "+VER",
        .var = vars_ver,
        .var_num = sizeof(vars_ver) / sizeof(vars_ver[0]),
    },
    {
        .name = "+IDFVER",
        .var = vars_idf_ver,
        .var_num = sizeof(vars_idf_ver) / sizeof(vars_idf_ver[0]),
    },
    {
        .name = "+BUILDTIME",
        .var = vars_buildtime,
        .var_num = sizeof(vars_buildtime) / sizeof(vars_buildtime[0]),
    },
    {
        .name = "+TEMP",
        .var = vars_temperature,
        .var_num = sizeof(vars_temperature) / sizeof(vars_temperature[0]),
    },
    {
        .name = "+TIME",
        .var = vars_time,
        .var_num = sizeof(vars_time) / sizeof(vars_time[0]),
    },
};

struct cat_command_group at_cmd_system_group = {
    .cmd = cmds,
    .cmd_num = sizeof(cmds) / sizeof(cmds[0]),
};
