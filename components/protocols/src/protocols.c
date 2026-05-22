#include "protocols.h"

#include "http.h"
#include "ota.h"
#include "scheduler.h"

void protocols_init(void)
{
    scheduler_init();
    http_init();
    ota_init();
}