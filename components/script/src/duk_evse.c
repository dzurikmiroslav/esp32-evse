
#include "duk_evse.h"
#include "evse.h"
#include "esp_log.h"


static duk_ret_t get_state(duk_context* ctx)
{
    duk_push_int(ctx, evse_get_state());
    return 1;
}

static duk_ret_t get_error(duk_context* ctx)
{
    duk_push_uint(ctx, evse_get_error());
    return 1;
}

static duk_ret_t get_enabled(duk_context* ctx)
{
    duk_push_boolean(ctx, evse_is_enabled());
    return 1;
}

static duk_ret_t set_enabled(duk_context* ctx)
{
    evse_set_enabled(duk_get_boolean(ctx, 0));
    return 0;
}

static duk_ret_t get_available(duk_context* ctx)
{
    duk_push_boolean(ctx, evse_is_available());
    return 1;
}

static duk_ret_t set_available(duk_context* ctx)
{
    evse_set_available(duk_get_boolean(ctx, 0));
    return 0;
}

static const duk_number_list_entry module_consts[] = {
   { "STATE_A", (double)(EVSE_STATE_A) },
   { "STATE_B1", (double)(EVSE_STATE_B1) },
   { "STATE_B2", (double)(EVSE_STATE_B2) },
   { "STATE_C1", (double)(EVSE_STATE_C1) },
   { "STATE_C2", (double)(EVSE_STATE_C2) },
   { "STATE_D1", (double)(EVSE_STATE_D1) },
   { "STATE_D2", (double)(EVSE_STATE_D2) },
   { "STATE_E", (double)(EVSE_STATE_E) },
   { "STATE_F", (double)(EVSE_STATE_E) },
   { "ERR_PILOT_FAULT_BIT", (double)(EVSE_ERR_PILOT_FAULT_BIT) },
   { "ERR_DIODE_SHORT_BIT", (double)(EVSE_ERR_DIODE_SHORT_BIT) },
   { "ERR_LOCK_FAULT_BIT", (double)(EVSE_ERR_LOCK_FAULT_BIT) },
   { "ERR_UNLOCK_FAULT_BIT", (double)(EVSE_ERR_UNLOCK_FAULT_BIT) },
   { "ERR_UNLOCK_FAULT_BIT", (double)(EVSE_ERR_UNLOCK_FAULT_BIT) },
   { "ERR_RCM_TRIGGERED_BIT", (double)(EVSE_ERR_RCM_TRIGGERED_BIT) },
   { "ERR_RCM_SELFTEST_FAULT_BIT", (double)(EVSE_ERR_RCM_SELFTEST_FAULT_BIT) },
   { "ERR_TEMPERATURE_HIGH_BIT", (double)(EVSE_ERR_TEMPERATURE_HIGH_BIT) },
   { "ERR_TEMPERATURE_FAULT_BIT", (double)(EVSE_ERR_TEMPERATURE_FAULT_BIT) },
   { NULL, 0.0 }
};


static duk_ret_t open_module(duk_context* ctx)
{
    duk_push_object(ctx);

    duk_put_number_list(ctx, -1, module_consts);

    duk_push_string(ctx, "state");
    duk_push_c_function(ctx, get_state, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER);

    duk_push_string(ctx, "error");
    duk_push_c_function(ctx, get_error, 0);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_GETTER);

    duk_push_string(ctx, "enabled");
    duk_push_c_function(ctx, get_enabled, 0);
    duk_push_c_function(ctx, set_enabled, 1);
    duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER);

    duk_push_string(ctx, "available");
    duk_push_c_function(ctx, get_available, 0);
    duk_push_c_function(ctx, set_available, 1);
    duk_def_prop(ctx, -4, DUK_DEFPROP_HAVE_GETTER | DUK_DEFPROP_HAVE_SETTER);

    duk_push_string(ctx, "_drivers");
    duk_push_array(ctx);
    duk_def_prop(ctx, -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_FORCE);

    return 1;
}

void duk_evse_init(duk_context* ctx)
{
    duk_push_c_function(ctx, open_module, 0 /*nargs*/);
    duk_call(ctx, 0);
    duk_put_global_string(ctx, "evse");
}