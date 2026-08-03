idf_component_register(
    SRCS "rest_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES homeguard_core esp_https_server json esp_timer
)
