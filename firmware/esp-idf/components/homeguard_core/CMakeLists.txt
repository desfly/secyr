set(HG_CORE_SRCS
    "../../../src/crc32.cpp"
    "../../../src/sha256.cpp"
    "../../../src/provisioning.cpp"
    "../../../src/provisioning_qr.cpp"
    "../../../src/hardware_profile.cpp"
    "../../../src/hardware_capabilities.cpp"
    "../../../src/device_identity.cpp"
    "../../../src/discovery.cpp"
    "../../../src/event_log.cpp"
    "../../../src/access_control.cpp"
    "../../../src/zone.cpp"
    "../../../src/pressure.cpp"
    "../../../src/health_monitor.cpp"
    "../../../src/telemetry.cpp"
)

idf_component_register(SRCS ${HG_CORE_SRCS} INCLUDE_DIRS "../../../include")
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Wpedantic)
