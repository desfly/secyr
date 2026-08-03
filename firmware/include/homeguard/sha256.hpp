#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace hg {
using Sha256Digest = std::array<uint8_t, 32>;
[[nodiscard]] Sha256Digest sha256(std::span<const std::byte> data);
[[nodiscard]] Sha256Digest sha256(std::string_view text);
[[nodiscard]] bool constant_time_equal(const Sha256Digest& lhs, const Sha256Digest& rhs);
[[nodiscard]] std::string sha256_hex(const Sha256Digest& digest);
}
