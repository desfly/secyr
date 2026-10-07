#pragma once

#include <atomic>

namespace homeguard::idf::ble_runtime_status {

inline std::atomic_bool runtime_ready{false};
inline std::atomic_bool advertising_active{false};

inline void set_ready(bool ready) {
    runtime_ready.store(ready, std::memory_order_release);
    if (!ready) advertising_active.store(false, std::memory_order_release);
}

inline bool ready() {
    return runtime_ready.load(std::memory_order_acquire);
}

inline void set_advertising(bool active) {
    advertising_active.store(active, std::memory_order_release);
}

inline bool advertising() {
    return advertising_active.load(std::memory_order_acquire);
}

}  // namespace homeguard::idf::ble_runtime_status
