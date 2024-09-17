
#include <nvs_flash.h>
#include <stdio.h>
#include <unity.h>

#include "evse.h"
#include "peripherals_mock.h"

void mock_reset(void)
{
    // EVSE:
    ac_relay_mock_state = false;
    pilot_mock_state = PILOT_MOCK_STATE_HIGH;
    // TODO reset errors

    // EV:
    pilot_mock_up_voltage = PILOT_VOLTAGE_12;
    pilot_mock_down_voltage_n12 = false;

    evse_process();
}

TEST_CASE("Standard charging sequence", "[evse]")
{
    mock_reset();

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

TEST_CASE("Pilot error", "[evse]")
{
    mock_reset();

    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);

    /**
     * Some invalid operation
     * A -> E
     */
    pilot_mock_up_voltage = PILOT_VOLTAGE_1;

    evse_process();
    TEST_ASSERT_EQUAL(EVSE_STATE_E, evse_get_state());
    TEST_ASSERT_TRUE(EVSE_ERR_PILOT_FAULT_BIT & evse_get_error());
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        printf("Erasing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    evse_init();

    printf("\n#### Running all tests #####\n\n");

    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();

    unity_run_menu();
}
