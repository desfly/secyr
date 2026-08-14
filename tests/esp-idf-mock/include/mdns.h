#pragma once

#include "esp_err.h"
#include <cstddef>
#include <cstdint>

typedef struct mdns_txt_item_t {
    const char* key;
    const char* value;
} mdns_txt_item_t;

inline esp_err_t mdns_init() { return ESP_OK; }
inline void mdns_free() {}
inline esp_err_t mdns_hostname_set(const char*) { return ESP_OK; }
inline esp_err_t mdns_instance_name_set(const char*) { return ESP_OK; }
inline esp_err_t mdns_service_add(const char*, const char*, const char*, std::uint16_t,
                                  mdns_txt_item_t*, std::size_t) { return ESP_OK; }
inline esp_err_t mdns_service_remove(const char*, const char*) { return ESP_OK; }
