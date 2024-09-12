#include "peripherals.h"

#include "ac_relay.h"
#include "adc.h"
#include "aux_io.h"
#include "energy_meter.h"
#include "led.h"
#include "pilot.h"
#include "proximity.h"
#include "rcm.h"
#include "socket_lock.h"
#include "temp_sensor.h"

void peripherals_init(void)
{
    adc_init();
    pilot_init();
    proximity_init();
    ac_relay_init();
    socket_lock_init();
    rcm_init();
    energy_meter_init();
    led_init();
    aux_init();
    temp_sensor_init();
}