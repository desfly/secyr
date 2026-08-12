#pragma once

#include "esp_timer.h"

#include <cstdint>

namespace homeguard::idf {

inline std::uint64_t access_now_ms() noexcept {
    const auto micros = esp_timer_get_time();
    return micros > 0 ? static_cast<std::uint64_t>(micros / 1000) : 0U;
}

}  // namespace homeguard::idf
