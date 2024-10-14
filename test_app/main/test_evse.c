
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <unity.h>
#include <unity_fixture.h>

#include "evse.h"
#include "peripherals_mock.h"

TEST_GROUP(evse);

TEST_SETUP(evse)
{
    // EVSE:
    evse_reset();
    ac_relay_mock_state = false;
    pilot_mock_state = PILOT_MOCK_STATE_HIGH;

    // EV:
    pilot_mock_up_voltage = PILOT_VOLTAGE_12;
    pilot_mock_down_voltage_n12 = false;
    energy_meter_mock_power = 0;
    energy_meter_mock_charging_time = 0;
    energy_meter_mock_consumption = 0;

    evse_process();

    // to make sure
    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST_TEAR_DOWN(evse)
{}

/**
 * EV connect, EVSE stay in B2 state
 */
void ev_connect_sequence()
{
    /**
     * EV connect
     * A -> B1
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_9;

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * When all conditions are met (avalable, enabled, autorized...)
     * B1 -> B2
     */
    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST(evse, ev_end_charging)
{
    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV request pause charging
     * C2 -> B2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_9;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_B2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV disconnect
     * B2 -> A
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_12;
    // pilot_mock_down_voltage_n12 = false;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST(evse, during_charging_disable_ev_react)
{
    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EVSE disable, EV has 6sec to end charging
     * C2 -> C1
     */
    evse_set_enabled(false);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV end charging
     * C1 -> B1
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_9;
    pilot_mock_down_voltage_n12 = false;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EVSE enable
     * B1 -> B2
     */
    evse_set_enabled(true);

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST(evse, during_charging_disable_ev_not_react)
{
    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EVSE disable, EV has 6sec to end charging
     * C2 -> C1
     */
    evse_set_enabled(false);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV not end charging, EVSE still connected AC
     * C1
     */
    vTaskStepTick(pdMS_TO_TICKS(1000));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV not end charging, EVSE force disconnect AC
     * C1
     */
    vTaskStepTick(pdMS_TO_TICKS(5500));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST(evse, before_charging_disable)
{
    /**
     * EVSE set not available
     * A
     */
    evse_set_enabled(false);

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV connect
     * A -> B1
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_9;

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * Still disabled
     * B1
     */
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EVSE enable
     * B1 -> B2
     */
    evse_set_enabled(true);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_B2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST(evse, during_charging_not_available_ev_react)
{
    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EVSE set not available, EV has 6sec to end charging
     * C2 -> C1
     */
    evse_set_available(false);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV end charging
     * C1 -> B1
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_9;
    pilot_mock_down_voltage_n12 = false;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EVSE in next iteration go to F state
     * B1 -> F
     */
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_F, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_LOW, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EVSE set available
     * F -> A
     */
    evse_set_available(true);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV still connected, next iteration it detect
     * A -> B1
     */
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * When all conditions are met (avalable, enabled, autorized...)
     * B1 -> B2
     */
    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST(evse, during_charging_not_available_ev_not_react)
{
    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EVSE go to no available, EV has 6sec to end charging
     * C2 -> C1
     */
    evse_set_available(false);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV not end charging, EVSE still connected AC
     * C1
     */
    vTaskStepTick(pdMS_TO_TICKS(1000));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV not end charging, EVSE force disconnect AC
     * C1- > F
     */
    vTaskStepTick(pdMS_TO_TICKS(5500));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_F, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_LOW, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST(evse, before_charging_not_available)
{
    /**
     * EVSE set not available
     * A -> F
     */
    evse_set_available(false);

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_F, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_LOW, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV connect
     * F
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_9;

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_F, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_LOW, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EVSE set avaialble, first iteration go to A
     * F -> A
     */
    evse_set_available(true);

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EVSE set avaialble
     * A -> B1
     */
    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * When all conditions are met (avalable, enabled, autorized...)
     * B1 -> B2
     */
    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST(evse, require_auth)
{
    evse_set_require_auth(true);
    TEST_ASSERT_FALSE(evse_is_pending_auth());

    /**
     * EV connect
     * A -> B1
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_9;

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
    TEST_ASSERT_TRUE(evse_is_pending_auth());

    /**
     * Wait for authorization
     * B1
     */
    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B1, evse_get_state());
    TEST_ASSERT_TRUE(evse_is_pending_auth());

    /**
     * When all conditions are met (avalable, enabled, autorized...)
     * B1 -> B2
     */
    evse_authorize();

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_B2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST(evse, pilot_error)
{
    /**
     * Some invalid operation
     * A -> E
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_1;

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_E, evse_get_state());
    TEST_ASSERT_TRUE(EVSE_ERR_PILOT_FAULT_BIT & evse_get_error());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_LOW, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * Stay in error
     * E
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_12;
    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_E, evse_get_state());
    TEST_ASSERT_TRUE(EVSE_ERR_PILOT_FAULT_BIT & evse_get_error());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_LOW, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * After 1min error cleared
     * E -> A
     */
    vTaskStepTick(pdMS_TO_TICKS(61000));

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(0, evse_get_error());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST(evse, charging_time_limit)
{
    uint32_t limit = 3600;  // 1h (in s)
    evse_set_charging_time_limit(limit);

    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit not reached yet
     */
    energy_meter_mock_charging_time = limit - 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit reached, stop charging
     * C2 -> C1
     */
    energy_meter_mock_charging_time = limit + 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Increase limit, start charging
     * C1 -> C2
     */
    limit *= 2;
    evse_set_charging_time_limit(limit);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit reached, stop charging
     * C2 -> C1
     */
    energy_meter_mock_charging_time = limit + 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST(evse, consumption_limit)
{
    uint32_t limit = 3600000;  // 1kWh (in Ws)
    evse_set_consumption_limit(limit);

    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit not reached yet
     */
    energy_meter_mock_consumption = limit - 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit reached, stop charging
     * C2 -> C1
     */
    energy_meter_mock_consumption = limit + 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Increase limit, start charging
     * C1 -> C2
     */
    limit *= 2;
    evse_set_consumption_limit(limit);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit reached, stop charging
     * C2 -> C1
     */
    energy_meter_mock_consumption = limit + 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST(evse, under_power_limit)
{
    uint16_t limit = 10000;  // 10kW (in W)
    evse_set_under_power_limit(limit);

    /**
     * EV connect
     * A -> B2
     */
    ev_connect_sequence();

    /**
     * EV require charging
     * B2 -> C2
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_6;
    pilot_mock_down_voltage_n12 = true;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit not reached yet
     */
    vTaskStepTick(pdMS_TO_TICKS(61000));  // must be 1min under limit to react

    energy_meter_mock_power = limit + 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit reached, not react yet
     * C2
     */
    energy_meter_mock_power = limit - 10;
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * Limit reached, not react yet
     * C2
     */
    vTaskStepTick(pdMS_TO_TICKS(59000));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C2, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_PWM, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * After 1min react
     * C2 -> C1
     */
    vTaskStepTick(pdMS_TO_TICKS(2000));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);
}

TEST_GROUP_RUNNER(evse)
{
    RUN_TEST_CASE(evse, ev_end_charging);
    RUN_TEST_CASE(evse, during_charging_disable_ev_react);
    RUN_TEST_CASE(evse, during_charging_disable_ev_not_react);
    RUN_TEST_CASE(evse, before_charging_disable);
    RUN_TEST_CASE(evse, during_charging_not_available_ev_react);
    RUN_TEST_CASE(evse, during_charging_not_available_ev_not_react);
    RUN_TEST_CASE(evse, before_charging_not_available);
    RUN_TEST_CASE(evse, require_auth);
    RUN_TEST_CASE(evse, pilot_error);
    RUN_TEST_CASE(evse, charging_time_limit);
    RUN_TEST_CASE(evse, consumption_limit);
    RUN_TEST_CASE(evse, under_power_limit);
}
