#pragma once

#include <cstddef>
#include <cstdint>

inline std::uint32_t& esp_mock_random_state()
{
    static std::uint32_t state = 0x6d2b79f5U;
    return state;
}

inline std::uint32_t esp_random()
{
    auto& state = esp_mock_random_state();
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

inline void esp_fill_random(void* buffer, std::size_t length)
{
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < length; ++i) {
        if ((i & 3U) == 0U) value = esp_random();
        bytes[i] = static_cast<std::uint8_t>(value >> ((i & 3U) * 8U));
    }
}
