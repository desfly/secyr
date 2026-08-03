idf_component_register(
    SRCS
        "app_main.cpp"
        "hg_build_info.cpp"
        "hg_build_http.cpp"
        "hg_i2c_bus.cpp"
        "hg_ads1115.cpp"
        "hg_mcp23017.cpp"
        "hg_ina226.cpp"
        "hg_ds3231.cpp"
        "hg_sd_storage.cpp"
        "hg_w5500.cpp"
        "hg_onewire_runtime.cpp"
        "hg_rs485.cpp"
        "hg_hardware_bootstrap.cpp"
        "hg_telemetry_runtime.cpp"
        "hg_infrastructure_http.cpp"
        "../../src/hardware_runtime.cpp"
        "../../src/device_command_router.cpp"
        "../../src/device_api_model.cpp"
        "../../src/hardware_calibration.cpp"
        "../../src/hardware_api.cpp"
        "../../src/infrastructure_status.cpp"
        "../../src/modbus_request.cpp"
    INCLUDE_DIRS
        "."
        "../../include"
    REQUIRES
        app_update
        driver
        esp_eth
        esp_event
        esp_http_server
        esp_netif
        fatfs
        nvs_flash
        sdmmc
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)


if(DEFINED ENV{GITHUB_SHA})
    set(HG_GIT_REVISION "$ENV{GITHUB_SHA}")
else()
    execute_process(
        COMMAND git rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../../.."
        OUTPUT_VARIABLE HG_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT HG_GIT_REVISION)
        set(HG_GIT_REVISION "unknown")
    endif()
endif()

string(TIMESTAMP HG_BUILD_TIMESTAMP_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)

target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
        HG_GIT_REVISION="${HG_GIT_REVISION}"
        HG_BUILD_TIMESTAMP_UTC="${HG_BUILD_TIMESTAMP_UTC}"
)
