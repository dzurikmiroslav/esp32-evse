#include "at.h"
#include "wifi.h"

void at_task_context_clean(at_task_context_t *context)
{
    if (context->wifi_scan_ap_list) {
        wifi_scan_aps_free(context->wifi_scan_ap_list);
    }
}