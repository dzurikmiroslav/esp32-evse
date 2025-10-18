#include "network.h"

#include "discovery.h"
#include "wifi.h"

void network_init(void)
{
    wifi_init();
    discovery_init();
}