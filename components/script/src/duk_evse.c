
#include "duk_evse.h"
#include "evse.h"
#include "esp_log.h"

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
    ESP_LOGI("duk", "top0 %d", duk_get_top(ctx));
    duk_push_object(ctx);

     ESP_LOGI("duk", "top1 %d", duk_get_top(ctx));
     duk_put_number_list(ctx, -1, module_consts);

    ESP_LOGI("duk", "top2 %d", duk_get_top(ctx));

    duk_push_string(ctx, "enabled");
    duk_push_c_function(ctx, get_enabled, 0);
    duk_push_c_function(ctx, set_enabled, 1);

     ESP_LOGI("duk", "top3 %d", duk_get_top(ctx));
    duk_def_prop(ctx,
        -4,
        DUK_DEFPROP_HAVE_GETTER |
        DUK_DEFPROP_HAVE_SETTER);

      ESP_LOGI("duk", "top4 %d", duk_get_top(ctx));

    return 1;
}

void duk_evse_init(duk_context* ctx)
{
    duk_push_c_function(ctx, open_module, 0 /*nargs*/);
    duk_call(ctx, 0);
    duk_put_global_string(ctx, "evse");
}