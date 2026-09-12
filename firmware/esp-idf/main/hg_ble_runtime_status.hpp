#pragma once

#include <atomic>

namespace homeguard::idf::ble_runtime_status {

inline std::atomic_bool runtime_ready{false};

inline void set_ready(bool ready) {
    runtime_ready.store(ready, std::memory_order_release);
}

inline bool ready() {
    return runtime_ready.load(std::memory_order_acquire);
}

}  // namespace homeguard::idf::ble_runtime_status
