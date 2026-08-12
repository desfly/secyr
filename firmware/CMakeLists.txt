cmake_minimum_required(VERSION 3.20)
project(homeguard_s3_build0060 LANGUAGES CXX)
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
    src/user_zone_access.cpp
    src/user_output_access.cpp
    src/config_exchange.cpp
    src/config_import.cpp
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
    ../tests/test_build0060_config.cpp
)
target_link_libraries(homeguard_tests PRIVATE homeguard_core)
enable_testing()
add_test(NAME homeguard_tests COMMAND homeguard_tests)
