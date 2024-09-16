idf_component_get_property(original_peripherals_dir peripherals COMPONENT_OVERRIDEN_DIR)

set(srcs
    "src/pilot_mock.c"
    "src/led_mock.c"
    "src/energy_meter_mock.c"
    "src/ac_relay_mock.c"
    "src/proximity_mock.c"
    "src/rcm_mock.c"
    "src/socket_lock_mock.c"
    "src/temp_sensor_mock.c")

idf_component_register(SRCS "${srcs}"
    INCLUDE_DIRS "${original_peripherals_dir}/include" "include")