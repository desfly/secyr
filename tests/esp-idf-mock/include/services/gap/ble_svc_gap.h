#pragma once
#include "host/ble_hs.h"

inline const char*& ble_mock_gap_name_storage() {
    static const char* value = "";
    return value;
}

inline void ble_svc_gap_init() {}
inline int ble_svc_gap_device_name_set(const char* name) {
    ble_mock_gap_name_storage() = name ? name : "";
    return 0;
}
inline const char* ble_svc_gap_device_name() {
    return ble_mock_gap_name_storage();
}
