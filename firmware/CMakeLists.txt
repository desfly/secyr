cmake_minimum_required(VERSION 3.20)
project(homeguard_s3 LANGUAGES CXX)
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
    src/device_identity.cpp
    src/discovery.cpp
    src/event_log.cpp
    src/system_model.cpp
    src/system_api.cpp
    src/output_interlock.cpp
    src/output_command.cpp
    src/physical_output_runtime.cpp
    src/access_control.cpp
    src/access_store.cpp
    src/zone.cpp
    src/pressure.cpp
    src/health_monitor.cpp
    src/telemetry.cpp
    src/power_monitor.cpp
    src/boot_self_test.cpp
    src/alarm_acknowledgement.cpp
    src/alarm_ack_command.cpp
    src/alarm_ack_api.cpp
    src/alarm_ack_event.cpp
    src/alarm_ack_telemetry.cpp
)
target_include_directories(homeguard_core PUBLIC include)

add_executable(homeguard_tests
    ../tests/test_main.cpp
    ../tests/test_zone.cpp
    ../tests/test_pressure.cpp
    ../tests/test_reliability.cpp
    ../tests/test_connectivity.cpp
    ../tests/test_provisioning.cpp
    ../tests/test_reset_sequence.cpp
    ../tests/test_alarm_ack.cpp
    ../tests/test_access_control.cpp
    ../tests/test_access_permissions.cpp
    ../tests/test_output_safety.cpp
    ../tests/test_readiness.cpp
    ../tests/test_build0009.cpp
    ../tests/test_build0032.cpp
    ../tests/test_build0038.cpp
    ../tests/test_build0045.cpp
    ../tests/test_build0046.cpp
)
target_link_libraries(homeguard_tests PRIVATE homeguard_core)
enable_testing()
add_test(NAME homeguard_tests COMMAND homeguard_tests)
