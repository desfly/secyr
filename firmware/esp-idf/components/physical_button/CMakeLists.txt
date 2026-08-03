idf_component_register(
    SRCS "physical_button.cpp"
    INCLUDE_DIRS "include"
    REQUIRES homeguard_core driver esp_timer freertos
)
