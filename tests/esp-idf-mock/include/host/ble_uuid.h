#pragma once

#include <cstdint>

struct ble_uuid_t {
    std::uint8_t type{};
};

struct ble_uuid128_t {
    ble_uuid_t u{};
    std::uint8_t value[16]{};
};

#define BLE_UUID128_INIT(...) {{128U}, {__VA_ARGS__}}
