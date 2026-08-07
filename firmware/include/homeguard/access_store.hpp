#pragma once

#include "homeguard/access_control.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace homeguard {

class AccessStoreCodec {
public:
    static constexpr std::size_t header_size = 8;
    static constexpr std::size_t record_size = 106;
    static constexpr std::size_t crc_size = 4;
    static constexpr std::size_t image_size = header_size + AccessControl::user_capacity * record_size + crc_size;
    using Image = std::array<std::byte, image_size>;

    [[nodiscard]] static Image encode(const AccessControl& access);
    [[nodiscard]] static bool decode(std::span<const std::byte> image, AccessControl& access);
};

}  // namespace homeguard
