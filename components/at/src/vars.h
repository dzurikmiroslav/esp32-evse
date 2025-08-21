#ifndef VARS_H_
#define VARS_H_

#include <stdint.h>

#define DEF_AT_VARS_RO(vars_name, var_type, var_data, getter_func) \
    static int vars_name##_read(const struct cat_variable* var)    \
    {                                                              \
        var_data = getter_func();                                  \
        return 0;                                                  \
    }                                                              \
                                                                   \
    static struct cat_variable vars_name[] = {                     \
        {                                                          \
            .type = var_type,                                      \
            .data = &var_data,                                     \
            .data_size = sizeof(var_data),                         \
            .access = CAT_VAR_ACCESS_READ_ONLY,                    \
            .read = vars_name##_read,                              \
        },                                                         \
    };

#define DEF_AT_VARS_RW_NO_CHECK(vars_name, var_type, var_data, getter_func, setter_func)  \
    static int vars_name##_read(const struct cat_variable* var)                           \
    {                                                                                     \
        var_data = getter_func();                                                         \
        return 0;                                                                         \
    }                                                                                     \
                                                                                          \
    static int vars_name##_write(const struct cat_variable* var, const size_t write_size) \
    {                                                                                     \
        setter_func(var_data);                                                            \
        return 0;                                                                         \
    }                                                                                     \
                                                                                          \
    static struct cat_variable vars_name[] = {                                            \
        {                                                                                 \
            .type = var_type,                                                             \
            .data = &var_data,                                                            \
            .data_size = sizeof(var_data),                                                \
            .access = CAT_VAR_ACCESS_READ_WRITE,                                          \
            .read = vars_name##_read,                                                     \
            .write = vars_name##_write,                                                   \
        },                                                                                \
    };

#define DEF_AT_VARS_RW_CHECK(vars_name, var_type, data_var, getter_func, setter_func)     \
    static int vars_name##_read(const struct cat_variable* var)                           \
    {                                                                                     \
        data_var = getter_func();                                                         \
        return 0;                                                                         \
    }                                                                                     \
                                                                                          \
    static int vars_name##_write(const struct cat_variable* var, const size_t write_size) \
    {                                                                                     \
        return setter_func(data_var) != ESP_OK;                                           \
    }                                                                                     \
                                                                                          \
    static struct cat_variable vars_name[] = {                                            \
        {                                                                                 \
            .type = var_type,                                                             \
            .data = &data_var,                                                            \
            .data_size = sizeof(data_var),                                                \
            .access = CAT_VAR_ACCESS_READ_WRITE,                                          \
            .read = vars_name##_read,                                                     \
            .write = vars_name##_write,                                                   \
        },                                                                                \
    };

extern int8_t var_i8_1;

extern uint8_t var_u8_1;
extern uint8_t var_u8_2;
extern uint8_t var_u8_3;
extern uint8_t var_u8_4;

extern uint16_t var_u16_1;

extern int32_t var_i32_1;
extern int32_t var_i32_2;
extern int32_t var_i32_3;

extern uint32_t var_u32_1;
extern uint32_t var_u32_2;

extern char var_str32_1[32];
extern char var_str32_2[32];

#endif /* VARS_H_ */