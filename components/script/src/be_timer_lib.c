#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "be_object.h"
#include "be_gc.h"
#include "be_vm.h"
#include "be_exec.h"

#include "evse.h"
#include "esp_log.h"

#define MAX_INTERVAL    5

typedef struct
{
    int delay;
    TickType_t prev_tick;
    bvalue callback;
} timer_interval_t;

typedef struct
{
    timer_interval_t intervals[MAX_INTERVAL];
} timer_ctx_t;

static int m_init(bvm* vm)
{
    timer_ctx_t* ctx = (timer_ctx_t*)malloc(sizeof(timer_ctx_t));
    for (int i = 0; i < MAX_INTERVAL; i++) {
        ctx->intervals[i].delay = 0;
    }

    be_newcomobj(vm, ctx, &be_commonobj_destroy_generic);
    be_setmember(vm, 1, ".p");

    be_return(vm);
}

static int m_process(bvm* vm)
{
    be_getmember(vm, 1, ".p");
    timer_ctx_t* ctx = (timer_ctx_t*)be_tocomptr(vm, -1);

    TickType_t now = xTaskGetTickCount();
    for (int i = 0;i < MAX_INTERVAL; i++) {
        timer_interval_t* interval = &ctx->intervals[i];

        if (interval->delay > 0) {
            if (now - interval->prev_tick >= pdMS_TO_TICKS(interval->delay)) {
                interval->prev_tick = now;

                bvalue* top = be_incrtop(vm);
                *top = interval->callback;
                int ret = be_pcall(vm, 0);
                if (ret > 0) {
                    be_dumpexcept(vm);
                }
                be_pop(vm, 1);
            }
        }
    }
    
    be_return(vm);
}

static int m_delay(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 1 && be_isnumber(vm, 1)) {
        vTaskDelay(pdMS_TO_TICKS(be_toint(vm, 1)));
        be_return(vm);
    }

    be_raise(vm, "type_error", NULL);
    be_return(vm);
}

static int m_set_interval(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 3 && be_isnumber(vm, 2) && be_isfunction(vm, 3)) {
        be_getmember(vm, 1, ".p");
        timer_ctx_t* ctx = (timer_ctx_t*)be_tocomptr(vm, -1);
        bool added = false;
        for (int i = 0; i < MAX_INTERVAL; i++) {
            timer_interval_t* interval = &ctx->intervals[i];
            if (interval->delay == 0) {
                interval->delay = be_toint(vm, 2);
                interval->prev_tick = xTaskGetTickCount();
                bvalue* callback = be_indexof(vm, 3);
                if (be_isgcobj(callback)) {
                    be_gc_fix_set(vm, callback->v.gc, btrue);
                }
                interval->callback = *callback;
                added = true;
                break;
            }

            if (!added) {
                be_raise(vm, "value_error", "max intervals reached");
            }
        }
    }

    be_return_nil(vm);
}

static int m_clear_interval(bvm* vm)
{
    be_return(vm);
}

/* @const_object_info_begin
class class_timer (scope: global, name: timer_class) {
    .p, var
    init, func(m_init)
    process, func(m_process)
    delay, func(m_delay)
    set_interval, func(m_set_interval)
    clear_interval, func(m_clear_interval)
}
@const_object_info_end */
#include "../generate/be_fixed_class_timer.h"

extern const bclass class_timer;

static int m_module_init(bvm* vm) {
    be_pushntvclass(vm, &class_timer);
    be_call(vm, 0);
    be_return(vm);
}

/* @const_object_info_begin
module timer (scope: global) {
    init, func(m_module_init)
}
@const_object_info_end */
#include "../generate/be_fixed_timer.h"