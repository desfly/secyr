set(HG_CORE_SRCS
    "../../../src/crc32.cpp"
    "../../../src/sha256.cpp"
    "../../../src/provisioning.cpp"
    "../../../src/provisioning_qr.cpp"
    "../../../src/hardware_profile.cpp"
    "../../../src/hardware_capabilities.cpp"
    "../../../src/service_button.cpp"
    "../../../src/startup_flow.cpp"
    "../../../src/device_identity.cpp"
    "../../../src/discovery.cpp"
    "../../../src/cloud_link.cpp"
    "../../../src/event_log.cpp"
    "../../../src/access_control.cpp"
    "../../../src/zone.cpp"
    "../../../src/pressure.cpp"
    "../../../src/challenge.cpp"
    "../../../src/idempotency.cpp"
    "../../../src/health_monitor.cpp"
    "../../../src/network_failover.cpp"
    "../../../src/telemetry.cpp"
    "../../../src/telemetry_transport.cpp"
    "../../../src/maintenance.cpp"
    "../../../src/power_monitor.cpp"
    "../../../src/command.cpp"
    "../../../src/boot_self_test.cpp"
    "../../../src/alarm_acknowledgement.cpp"
    "../../../src/alarm_ack_command.cpp"
    "../../../src/alarm_ack_api.cpp"
    "../../../src/alarm_ack_event.cpp"
    "../../../src/alarm_ack_telemetry.cpp"
)
set(HOMEGUARD_BUILD0017_SOURCES
    "../../../src/supervised_loop.cpp"
    "../../../src/pt506_filter.cpp"
    "../../../src/valve_machine.cpp"
    "../../../src/temperature_statistics.cpp"
)

idf_component_register(
    SRCS ${HG_CORE_SRCS}
    INCLUDE_DIRS "../../../include"
    PRIV_REQUIRES mbedtls esp_timer
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic -O3)
