idf_component_register(
    SRCS "provisioning_https.cpp"
    INCLUDE_DIRS "include"
    REQUIRES provisioning_service nvs_config_store esp_https_server json esp_timer
)
