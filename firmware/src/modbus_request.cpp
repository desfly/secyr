#include "homeguard/modbus_request.hpp"

namespace homeguard {

std::uint16_t modbus_crc16_portable(
    const std::uint8_t* data,
    std::size_t size) noexcept
{
    std::uint16_t crc = 0xFFFF;

    for (std::size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) != 0 ?
                static_cast<std::uint16_t>(
                    (crc >> 1) ^ 0xA001U) :
                static_cast<std::uint16_t>(crc >> 1);
        }
    }

    return crc;
}

std::array<std::uint8_t, 8> modbus_read_holding_registers(
    std::uint8_t slave,
    std::uint16_t first_register,
    std::uint16_t register_count)
{
    std::array<std::uint8_t, 8> request{
        slave,
        0x03,
        static_cast<std::uint8_t>(first_register >> 8),
        static_cast<std::uint8_t>(first_register & 0xFF),
        static_cast<std::uint8_t>(register_count >> 8),
        static_cast<std::uint8_t>(register_count & 0xFF),
        0,
        0,
    };

    const auto crc =
        modbus_crc16_portable(
            request.data(),
            6);

    request[6] =
        static_cast<std::uint8_t>(crc & 0xFF);
    request[7] =
        static_cast<std::uint8_t>(crc >> 8);
    return request;
}

}  // namespace homeguard
