#include "protocols.h"
#include "date_time.h"
#include "rest.h"
#include "mqtt.h"
#include "modbus_tcp.h"

void protocols_init(void)
{
    date_time_init();
    rest_init();
    mqtt_init();
    modbus_tcp_init();
}