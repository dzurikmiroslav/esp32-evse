#include "protocols.h"

#include "http.h"
#include "modbus_tcp.h"
#include "scheduler.h"

void protocols_init(void)
{
    scheduler_init();
    http_init();
    modbus_tcp_init();
}