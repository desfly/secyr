#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace hg { uint32_t crc32(std::span<const std::byte> data); }
