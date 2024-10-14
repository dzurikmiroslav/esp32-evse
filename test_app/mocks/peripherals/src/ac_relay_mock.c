#include "ac_relay.h"
#include "peripherals_mock.h"

bool ac_relay_mock_state;

void ac_relay_set_state(bool state)
{
    ac_relay_mock_state = state;
}