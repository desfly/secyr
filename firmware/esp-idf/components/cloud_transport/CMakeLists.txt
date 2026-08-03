idf_component_register(
    SRCS "cloud_transport.cpp"
    INCLUDE_DIRS "include"
    REQUIRES homeguard_core mqtt esp-tls mbedtls
)
