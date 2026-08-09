#pragma once

#include <cstddef>
#include <cstdint>

inline void esp_fill_random(void* buffer, std::size_t length)
{
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    static std::uint32_t state = 0x6d2b79f5U;
    for (std::size_t i = 0; i < length; ++i) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        bytes[i] = static_cast<std::uint8_t>(state & 0xffU);
    }
}
