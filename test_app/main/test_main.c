#include <nvs_flash.h>
#include <stdio.h>
#include <unity.h>

#include "evse.h"
#include "peripherals_mock.h"

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