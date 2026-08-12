#pragma once

#include <cstddef>
#include <cstdint>

// Host-only ESP-IDF random API stand-in. The production firmware uses the
// real esp_random component; tests only need a deterministic, link-safe
// implementation so access-management code can be compiled and linked.
inline std::uint32_t esp_random()
{
    static std::uint32_t state = 0x6d2b79f5U;
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

inline void esp_fill_random(void* buffer, std::size_t length)
{
    if (buffer == nullptr) return;
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    std::uint32_t value = 0U;
    for (std::size_t i = 0; i < length; ++i) {
        if ((i & 3U) == 0U) value = esp_random();
        bytes[i] = static_cast<std::uint8_t>(value >> ((i & 3U) * 8U));
    }
}
