#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "berry.h"

#include "evse.h"


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
        be_return(vm);
    }
    be_raise(vm, "type_error", NULL);
    be_return_nil(vm);
}

/* @const_object_info_begin
module evse (scope: global) {
    STATE_A, int(EVSE_STATE_A)
    STATE_B, int(EVSE_STATE_B)
    STATE_C, int(EVSE_STATE_C)
    STATE_D, int(EVSE_STATE_D)
    STATE_E, int(EVSE_STATE_E)
    STATE_F, int(EVSE_STATE_F)
    state, func(m_state)
    enabled, func(m_enabled)
    set_enabled, func(m_set_enabled)
}
@const_object_info_end */
#include "../generate/be_fixed_evse.h"