#include "protocols.h"
#include "date_time.h"
#include "http.h"
#include "mqtt.h"
#include "modbus_tcp.h"

void protocols_init(void)
{
    date_time_init();
    http_init();
    mqtt_init();
    modbus_tcp_init();
}