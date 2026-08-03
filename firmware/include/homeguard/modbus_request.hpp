#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard {

std::array<std::uint8_t, 8> modbus_read_holding_registers(
    std::uint8_t slave,
    std::uint16_t first_register,
    std::uint16_t register_count);

std::uint16_t modbus_crc16_portable(
    const std::uint8_t* data,
    std::size_t size) noexcept;

}  // namespace homeguard
