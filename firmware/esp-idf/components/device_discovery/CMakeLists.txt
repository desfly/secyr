idf_component_register(
    SRCS "device_discovery.cpp"
    INCLUDE_DIRS "include"
    REQUIRES homeguard_core mdns esp_netif lwip freertos
)
