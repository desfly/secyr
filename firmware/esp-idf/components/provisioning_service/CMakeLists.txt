idf_component_register(
    SRCS "provisioning_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES homeguard_core nvs_config_store setup_ap esp_hw_support
)
