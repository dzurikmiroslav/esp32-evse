
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <unity.h>

#include "evse.h"
#include "peripherals_mock.h"

void mock_reset(void)
{
    // EVSE:
    evse_reset();
    ac_relay_mock_state = false;
    pilot_mock_state = PILOT_MOCK_STATE_HIGH;

    // EV:
    pilot_mock_up_voltage = PILOT_VOLTAGE_12;
    pilot_mock_down_voltage_n12 = false;

    evse_process();

    // to make sure
    TEST_ASSERT_EQUAL(EVSE_STATE_A, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST_CASE("Standard charging, EV end charging", "[evse]")
{
    mock_reset();

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

TEST_CASE("Standard charging, EVSE no available, EV react in 6s", "[evse]")
{
    mock_reset();

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
     * EVSE go to no available, EV has 3sec to end charging
     * C2 -> C1
     */
    evse_set_available(false);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV disconnect
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
}

TEST_CASE("Standard charging, EVSE no available, EV not react in 6s", "[evse]")
{
    mock_reset();

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
     * EVSE go to no available, EV has 3sec to end charging
     * C2 -> C1
     */
    evse_set_available(false);
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV not disconnect, EVSE still connected AC
     * C1
     */
    vTaskDelay(pdMS_TO_TICKS(1000));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_C1, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_HIGH, pilot_mock_state);
    TEST_ASSERT_TRUE(ac_relay_mock_state);

    /**
     * EV not disconnect, EVSE force disconnect AC
     * C1- > F
     */
    vTaskDelay(pdMS_TO_TICKS(5500));
    evse_process();

    TEST_ASSERT_EQUAL(EVSE_STATE_F, evse_get_state());
    TEST_ASSERT_EQUAL(PILOT_MOCK_STATE_LOW, pilot_mock_state);
    TEST_ASSERT_FALSE(ac_relay_mock_state);
}

TEST_CASE("Pilot error", "[evse]")
{
    mock_reset();

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

    // printf("\n#### Running all tests #####\n\n");

    // UNITY_BEGIN();
    // unity_run_all_tests();
    // UNITY_END();

    unity_run_menu();
}