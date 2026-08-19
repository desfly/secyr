idf_component_register(
    SRCS "websocket_telemetry.cpp"
    INCLUDE_DIRS "include"
    REQUIRES homeguard_core esp_http_server esp_https_server esp_timer
)