#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "be_object.h"
#include "be_gc.h"
#include "be_vm.h"
#include "be_exec.h"
#include "esp_log.h"
#include "sys/queue.h"

#include "evse.h"
#include "energy_meter.h"
#include "script_utils.h"

typedef struct driver_entry_s {
    SLIST_ENTRY(driver_entry_s) entries;
    bvalue value;
} driver_entry_t;

typedef struct
{
    TickType_t tick_100ms;
    TickType_t tick_250ms;
    TickType_t tick_1s;
    SLIST_HEAD(drivers_head, driver_entry_s) drivers;
} evse_ctx_t;

static int m_init(bvm* vm)
{
    evse_ctx_t* ctx = (evse_ctx_t*)malloc(sizeof(evse_ctx_t));
    ctx->tick_100ms = 0;
    ctx->tick_250ms = 0;
    ctx->tick_1s = 0;
    SLIST_INIT(&ctx->drivers);

    be_newcomobj(vm, ctx, &be_commonobj_destroy_generic);
    be_setmember(vm, 1, "_ctx");

    be_return(vm);
}

static int m_state(bvm* vm)
{
    be_pushint(vm, evse_get_state());
    be_return(vm);
}

static int m_enabled(bvm* vm)
{
    be_pushbool(vm, evse_is_enabled());
    be_return(vm);
}

static int m_set_enabled(bvm* vm)
{
    int top = be_top(vm);
    if (top == 2 && be_isbool(vm, 2)) {
        evse_set_enabled(be_tobool(vm, 2));
    } else {
        be_raise(vm, "type_error", NULL);
    }
    be_return(vm);
}

static int m_charging_current(bvm* vm)
{
    be_pushreal(vm, evse_get_charging_current() / 10.0f);
    be_return(vm);
}

static int m_set_charging_current(bvm* vm)
{
    int top = be_top(vm);
    if (top == 2 && be_isnumber(vm, 2)) {
        uint16_t value = round(be_toreal(vm, 2) * 10);
        if (evse_set_charging_current(value) != ESP_OK) {
            be_raise(vm, "value_error", "invalid value");
        }
    } else {
        be_raise(vm, "type_error", NULL);
    }
    be_return(vm);
}

static int m_power(bvm* vm)
{
    be_pushint(vm, energy_meter_get_power());
    be_return(vm);
}

static int m_charging_time(bvm* vm)
{
    be_pushint(vm, energy_meter_get_charging_time());
    be_return(vm);
}

static int m_session_time(bvm* vm)
{
    be_pushint(vm, energy_meter_get_session_time);
    be_return(vm);
}

static int m_consumption(bvm* vm)
{
    be_pushint(vm, energy_meter_get_consumption());
    be_return(vm);
}

static int m_voltage(bvm* vm)
{
    be_newobject(vm, "list");

    be_pushreal(vm, energy_meter_get_l1_voltage());
    be_data_push(vm, -2);
    be_pop(vm, 1);

    be_pushreal(vm, energy_meter_get_l2_voltage());
    be_data_push(vm, -2);
    be_pop(vm, 1);

    be_pushreal(vm, energy_meter_get_l3_voltage());
    be_data_push(vm, -2);
    be_pop(vm, 1);

    be_pop(vm, 1);
    be_return(vm);
}

static int m_current(bvm* vm)
{
    be_newobject(vm, "list");

    be_pushreal(vm, energy_meter_get_l1_current());
    be_data_push(vm, -2);
    be_pop(vm, 1);

    be_pushreal(vm, energy_meter_get_l2_current());
    be_data_push(vm, -2);
    be_pop(vm, 1);

    be_pushreal(vm, energy_meter_get_l3_current());
    be_data_push(vm, -2);
    be_pop(vm, 1);

    be_pop(vm, 1);
    be_return(vm);
}

static void driver_call_event(bvm* vm, const char* method)
{
    be_getmethod(vm, -1, method);
    if (!be_isnil(vm, -1)) {
        be_pushvalue(vm, -2);
        script_watchdog_reset();
        int ret = be_pcall(vm, 1);
        script_watchdog_disable();
        if (ret == BE_EXCEPTION) {
            script_handle_result(vm, ret);
        }
        be_pop(vm, 1);
    }
    be_pop(vm, 1);
}

static int m_process(bvm* vm)
{
    be_getmember(vm, 1, "_ctx");
    evse_ctx_t* ctx = (evse_ctx_t*)be_tocomptr(vm, -1);

    TickType_t now = xTaskGetTickCount();

    bool every_100ms = false;
    if (now - ctx->tick_100ms >= pdMS_TO_TICKS(100)) {
        ctx->tick_100ms = now;
        every_100ms = true;
    }

    bool every_250ms = false;
    if (now - ctx->tick_250ms >= pdMS_TO_TICKS(250)) {
        ctx->tick_250ms = now;
        every_250ms = true;
    }

    bool every_1s = false;
    if (now - ctx->tick_1s >= pdMS_TO_TICKS(1000)) {
        ctx->tick_1s = now;
        every_1s = true;
    }

    driver_entry_t* driver;
    SLIST_FOREACH(driver, &ctx->drivers, entries) {
        bvalue* top = be_incrtop(vm);
        *top = driver->value;

        driver_call_event(vm, "loop");
        if (every_100ms) {
            driver_call_event(vm, "every_100ms");
        }
        if (every_250ms) {
            driver_call_event(vm, "every_250ms");
        }
        if (every_1s) {
            driver_call_event(vm, "every_1s");
        }

        be_pop(vm, 1);
    }

    be_return(vm);
}

static int m_add_driver(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 2 && be_isinstance(vm, 2)) {
        be_getmember(vm, 1, "_ctx");
        evse_ctx_t* ctx = (evse_ctx_t*)be_tocomptr(vm, -1);

        bvalue* driver = be_indexof(vm, 2);
        if (be_isgcobj(driver)) {
            be_gc_fix_set(vm, driver->v.gc, btrue);
        }

        driver_entry_t* entry = (driver_entry_t*)malloc(sizeof(driver_entry_t));
        entry->value = *driver;
        SLIST_INSERT_HEAD(&ctx->drivers, entry, entries);
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

static int m_remove_driver(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 2 && be_isinstance(vm, 2)) {
        be_getmember(vm, 1, "_ctx");
        evse_ctx_t* ctx = (evse_ctx_t*)be_tocomptr(vm, -1);

        bvalue* driver = be_indexof(vm, 2);

        driver_entry_t* entry;
        driver_entry_t* entry_tmp;
        SLIST_FOREACH_SAFE(entry, &ctx->drivers, entries, entry_tmp) {
            if (memcmp(&entry->value, driver, sizeof(bvalue)) == 0) {
                ESP_LOGI("evseber", "remove entry" );
                SLIST_REMOVE(&ctx->drivers, entry, driver_entry_s, entries);
                free((void*)entry);
            }
        }
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

static int m_delay(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 1 && be_isnumber(vm, 1)) {
        vTaskDelay(pdMS_TO_TICKS(be_toint(vm, 1)));
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

/* @const_object_info_begin
class class_evse (scope: global, name: Evse) {
    STATE_A, int(EVSE_STATE_A)
    STATE_B, int(EVSE_STATE_B)
    STATE_C, int(EVSE_STATE_C)
    STATE_D, int(EVSE_STATE_D)
    STATE_E, int(EVSE_STATE_E)
    STATE_F, int(EVSE_STATE_F)

    _ctx, var

    init, func(m_init)

    _process, func(m_process)

    state, func(m_state)
    enabled, func(m_enabled)
    set_enabled, func(m_set_enabled)
    charging_current, func(m_charging_current)
    set_charging_current, func(m_set_charging_current)
    power, func(m_power)
    charging_time, func(m_charging_time)
    session_time, func(m_session_time)
    consumption, func(m_consumption)
    voltage, func(m_voltage)
    current, func(m_current)

    add_driver, func(m_add_driver)
    remove_driver, func(m_remove_driver)

    delay, func(m_delay)
}
@const_object_info_end */
#include "../generate/be_fixed_class_evse.h"

extern const bclass class_evse;

static int m_module_init(bvm* vm) {
    be_pushntvclass(vm, &class_evse);
    be_call(vm, 0);
    be_return(vm);
}

/* @const_object_info_begin
module evse (scope: global) {
    init, func(m_module_init)
}
@const_object_info_end */
#include "../generate/be_fixed_evse.h"