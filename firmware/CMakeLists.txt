cmake_minimum_required(VERSION 3.20)
project(homeguard_s3_build0013 LANGUAGES CXX)
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
    src/service_button.cpp
    src/startup_flow.cpp
    src/device_identity.cpp
    src/discovery.cpp
    src/cloud_link.cpp
    src/event_log.cpp
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
)
target_link_libraries(homeguard_tests PRIVATE homeguard_core)
enable_testing()
add_test(NAME homeguard_tests COMMAND homeguard_tests)
