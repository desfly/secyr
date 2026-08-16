cmake_minimum_required(VERSION 3.20)
project(homeguard_s3_build0058 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_library(homeguard_core
    src/crc32.cpp
    src/sha256.cpp
    src/provisioning.cpp
    src/provisioning_qr.cpp
    src/hardware_profile.cpp
    src/hardware_capabilities.cpp
    src/hardware_test.cpp
    src/commissioning.cpp
    src/commissioning_state.cpp
    src/hardware_verification.cpp
    src/boot_readiness.cpp
    src/service_readiness.cpp
    src/service_button.cpp
    src/startup_flow.cpp
    src/device_identity.cpp
    src/discovery.cpp
    src/cloud_link.cpp
    src/event_log.cpp
    src/system_model.cpp
    src/system_api.cpp
    src/output_interlock.cpp
    src/output_command.cpp
    src/physical_output_runtime.cpp
    src/access_control.cpp
    src/access_store.cpp
    src/device_api_model.cpp
    src/device_command_router.cpp
    src/zone.cpp
    src/pressure.cpp
    src/challenge.cpp
    src/idempotency.cpp
    src/health_monitor.cpp
    src/network_failover.cpp
    src/telemetry.cpp
    src/maintenance.cpp
    src/power_monitor.cpp
    src/command.cpp
    src/local_api.cpp
    src/controller.cpp
    src/boot_self_test.cpp
)
target_include_directories(homeguard_core PUBLIC include)

add_executable(homeguard_tests
    ../tests/test_main.cpp
    ../tests/test_zone.cpp
    ../tests/test_pressure.cpp
    ../tests/test_reliability.cpp
    ../tests/test_controller.cpp
    ../tests/test_connectivity.cpp
    ../tests/test_provisioning.cpp
    ../tests/test_build0009.cpp
    ../tests/test_build0013.cpp
    ../tests/test_build0032.cpp
    ../tests/test_build0037.cpp
    ../tests/test_build0038.cpp
    ../tests/test_build0045.cpp
    ../tests/test_build0046.cpp
    ../tests/test_build0047.cpp
    ../tests/test_build0048.cpp
    ../tests/test_build0049.cpp
    ../tests/test_build0050.cpp
    ../tests/test_build0051.cpp
    ../tests/test_build0052.cpp
    ../tests/test_build0053.cpp
    ../tests/test_build0054.cpp
    ../tests/test_build0055.cpp
    ../tests/test_build0056.cpp
    ../tests/test_build0057.cpp
    ../tests/test_build0058.cpp
    ../tests/test_reset_sequence.cpp
)
target_link_libraries(homeguard_tests PRIVATE homeguard_core)

# Exercise the actual ESP-IDF FactoryResetManager against an in-memory NVS/Wi-Fi
# mock. This is intentionally separate from the pure core test binary so the
# release gate validates namespace erasure and immutable-state preservation,
# not merely static source strings.
add_executable(factory_reset_host_test
    ../tests/test_factory_reset_host.cpp
    esp-idf/main/hg_factory_reset.cpp
    esp-idf/main/hg_commissioning_nvs.cpp
)
target_include_directories(factory_reset_host_test PRIVATE
    esp-idf/main
    ../tests/esp-idf-mock/include
)
target_link_libraries(factory_reset_host_test PRIVATE homeguard_core)

# Apply a decoded backup through the real persistence stores. A one-shot NVS
# write fault after Access/Wi-Fi mutation must roll every earlier write back,
# while the same transaction without the injected fault must commit normally.
add_executable(config_transaction_host_test
    ../tests/test_config_transaction_host.cpp
    esp-idf/main/hg_config_transaction.cpp
    esp-idf/main/hg_access_nvs.cpp
    esp-idf/main/hg_cloud_nvs.cpp
    esp-idf/main/hg_commissioning_nvs.cpp
    esp-idf/main/hg_network_http.cpp
    esp-idf/main/hg_network_persistence.cpp
)
target_include_directories(config_transaction_host_test PRIVATE
    esp-idf/main
    ../tests/esp-idf-mock/include
)
target_link_libraries(config_transaction_host_test PRIVATE homeguard_core)

enable_testing()
add_test(NAME homeguard_tests COMMAND homeguard_tests)
add_test(NAME factory_reset_host_test COMMAND factory_reset_host_test)
add_test(NAME config_transaction_host_test COMMAND config_transaction_host_test)
