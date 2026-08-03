idf_component_register(
    SRCS "network_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES homeguard_core esp_wifi esp_event esp_netif lwip
)
