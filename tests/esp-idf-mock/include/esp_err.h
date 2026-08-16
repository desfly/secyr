#pragma once
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_NO_MEM = 0x101;
constexpr esp_err_t ESP_ERR_INVALID_ARG = 0x102;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
constexpr esp_err_t ESP_ERR_INVALID_SIZE = 0x104;
constexpr esp_err_t ESP_ERR_NOT_FOUND = 0x105;
constexpr esp_err_t ESP_ERR_TIMEOUT = 0x107;
constexpr esp_err_t ESP_ERR_INVALID_RESPONSE = 0x108;
constexpr esp_err_t ESP_ERR_INVALID_CRC = 0x109;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
constexpr esp_err_t ESP_ERR_NVS_NO_FREE_PAGES = 0x110d;
constexpr esp_err_t ESP_ERR_NVS_NEW_VERSION_FOUND = 0x1110;
inline const char* esp_err_to_name(esp_err_t) { return "mock"; }
