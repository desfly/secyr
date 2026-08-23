#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

using nvs_handle_t = std::uint32_t;

enum nvs_open_mode_t {
    NVS_READONLY = 0,
    NVS_READWRITE = 1,
};

inline esp_err_t nvs_open(const char*, nvs_open_mode_t, nvs_handle_t* handle) {
    if (handle != nullptr) *handle = 1;
    return ESP_ERR_NVS_NOT_FOUND;
}

inline void nvs_close(nvs_handle_t) {}

inline esp_err_t nvs_get_blob(nvs_handle_t, const char*, void*, std::size_t*) {
    return ESP_ERR_NVS_NOT_FOUND;
}

inline esp_err_t nvs_set_blob(nvs_handle_t, const char*, const void*, std::size_t) {
    return ESP_OK;
}

inline esp_err_t nvs_get_str(nvs_handle_t, const char*, char*, std::size_t*) {
    return ESP_ERR_NVS_NOT_FOUND;
}

inline esp_err_t nvs_set_str(nvs_handle_t, const char*, const char*) {
    return ESP_OK;
}

inline esp_err_t nvs_get_u8(nvs_handle_t, const char*, std::uint8_t*) {
    return ESP_ERR_NVS_NOT_FOUND;
}

inline esp_err_t nvs_set_u8(nvs_handle_t, const char*, std::uint8_t) {
    return ESP_OK;
}

inline esp_err_t nvs_get_u32(nvs_handle_t, const char*, std::uint32_t*) {
    return ESP_ERR_NVS_NOT_FOUND;
}

inline esp_err_t nvs_set_u32(nvs_handle_t, const char*, std::uint32_t) {
    return ESP_OK;
}

inline esp_err_t nvs_erase_key(nvs_handle_t, const char*) {
    return ESP_ERR_NVS_NOT_FOUND;
}

inline esp_err_t nvs_erase_all(nvs_handle_t) {
    return ESP_OK;
}

inline esp_err_t nvs_commit(nvs_handle_t) {
    return ESP_OK;
}
