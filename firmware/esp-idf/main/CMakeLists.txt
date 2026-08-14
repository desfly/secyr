# Keep the approved Web UI in /web as the single source of truth.
# Copy it into the component directory at configure time so ESP-IDF can embed
# the exact same files into flash without maintaining a second checked-in copy.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/web")
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../web/index.html" "${CMAKE_CURRENT_LIST_DIR}/web/index.html" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../web/app.css" "${CMAKE_CURRENT_LIST_DIR}/web/app.css" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../web/app.js" "${CMAKE_CURRENT_LIST_DIR}/web/app.js" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../web/access-session.js" "${CMAKE_CURRENT_LIST_DIR}/web/access-session.js" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../web/bruce.jpg" "${CMAKE_CURRENT_LIST_DIR}/web/bruce.jpg" COPYONLY)

idf_component_register(
    SRCS
        "app_main.cpp"
        "hg_web_http.cpp"
        "hg_network_http.cpp"
        "hg_lan_http.cpp"
        "hg_cloud_link.cpp"
        "hg_cloud_http.cpp"
        "hg_cloud_nvs.cpp"
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
        "hg_pzem004t.cpp"
        "hg_hardware_bootstrap.cpp"
        "hg_telemetry_runtime.cpp"
        "hg_infrastructure_http.cpp"
        "hg_system_http.cpp"
        "hg_service_http.cpp"
        "hg_output_http.cpp"
        "hg_gpio_output_backend.cpp"
        "hg_access_nvs.cpp"
        "hg_access_http.cpp"
        "hg_commissioning_nvs.cpp"
        "../../src/system_model.cpp"
        "../../src/system_api.cpp"
        "../../src/output_interlock.cpp"
        "../../src/output_command.cpp"
        "../../src/physical_output_runtime.cpp"
        "../../src/event_log.cpp"
        "../../src/access_control.cpp"
        "../../src/access_store.cpp"
        "../../src/crc32.cpp"
        "../../src/sha256.cpp"
        "../../src/hardware_profile.cpp"
        "../../src/hardware_runtime.cpp"
        "../../src/hardware_test.cpp"
        "../../src/commissioning.cpp"
        "../../src/commissioning_state.cpp"
        "../../src/hardware_verification.cpp"
        "../../src/boot_readiness.cpp"
        "../../src/service_readiness.cpp"
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
        device_discovery
        driver
        esp_eth
        esp_event
        esp_http_server
        esp_netif
        esp_timer
        esp_wifi
        fatfs
        mqtt
        nvs_flash
        sdmmc
    EMBED_TXTFILES
        "web/index.html"
        "web/app.css"
        "web/app.js"
        "web/access-session.js"
    EMBED_FILES
        "web/bruce.jpg"
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

if(DEFINED ENV{GITHUB_RUN_NUMBER})
    set(HG_CI_BUILD_NUMBER "$ENV{GITHUB_RUN_NUMBER}")
else()
    set(HG_CI_BUILD_NUMBER "local")
endif()

string(TIMESTAMP HG_BUILD_TIMESTAMP_UTC "%Y-%m-%dT%H:%M:%SZ" UTC)

target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE
        HG_GIT_REVISION="${HG_GIT_REVISION}"
        HG_CI_BUILD_NUMBER="${HG_CI_BUILD_NUMBER}"
        HG_BUILD_TIMESTAMP_UTC="${HG_BUILD_TIMESTAMP_UTC}"
)
