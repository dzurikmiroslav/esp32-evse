#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "berry.h"

#include "aux_io.h"

static int m_write(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 2 && be_isstring(vm, 1) && be_isbool(vm, 2)) {
        const char* name = be_tostring(vm, 1);
        bool value = be_tobool(vm, 2);

        if (aux_write(name, value) == ESP_OK) {
            be_return(vm);
        } else {
            be_raise(vm, "value_error", "unknow output name");
            be_return_nil(vm);
        }
    }
    be_raise(vm, "type_error", NULL);
    be_return_nil(vm);
}

static int m_read(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 1 && be_isstring(vm, 1)) {
        const char* name = be_tostring(vm, 1);
        bool value;

        if (aux_read(name, &value) == ESP_OK) {
            be_pushbool(vm, value);
            be_return(vm);
        } else {
            be_raise(vm, "value_error", "unknow input name");
            be_return_nil(vm);
        }
    }
    be_raise(vm, "type_error", NULL);
    be_return_nil(vm);
}

static int m_analog_read(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 1 && be_isstring(vm, 1)) {
        const char* name = be_tostring(vm, 1);
        int value;

        if (aux_analog_read(name, &value) == ESP_OK) {
            be_pushint(vm, value);
            be_return(vm);
        } else {
            be_raise(vm, "value_error", "unknow input name");
            be_return_nil(vm);
        }
    }
    be_raise(vm, "type_error", NULL);
    be_return_nil(vm);
}

/* @const_object_info_begin
module aux (scope: global) {
    write, func(m_write)
    read, func(m_read)
    analog_read, func(m_analog_read)
}
@const_object_info_end */
#include "../generate/be_fixed_aux.h"