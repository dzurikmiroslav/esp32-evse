#include "protocols.h"
#include "rest.h"
#include "mqtt.h"
#include "tcp_logger.h"
#include "modbus_tcp.h"

void protocols_init(void)
{
    rest_init();
    mqtt_init();
    tcp_logger_init();
    modbus_tcp_init();
}