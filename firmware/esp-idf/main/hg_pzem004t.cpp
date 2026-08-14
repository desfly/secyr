#include "hg_pzem004t.hpp"
#include "hg_rs485.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {

namespace {

constexpr std::uint8_t kReadInputRegisters = 0x04;
constexpr std::uint16_t kFirstRegister = 0x0000;
constexpr std::uint16_t kRegisterCount = 10;
constexpr std::size_t kExpectedResponseSize = 25;

std::uint16_t be16(const std::uint8_t* data) noexcept
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[0]) << 8) |
        static_cast<std::uint16_t>(data[1]));
}

std::uint32_t pzem32(std::uint16_t low_word, std::uint16_t high_word) noexcept
{
    return static_cast<std::uint32_t>(low_word) |
        (static_cast<std::uint32_t>(high_word) << 16);
}

}  // namespace

Pzem004t::Pzem004t(Rs485Runtime* bus) noexcept : bus_(bus) {}

void Pzem004t::attach(Rs485Runtime* bus) noexcept
{
    bus_ = bus;
}

esp_err_t Pzem004t::read(Pzem004tReading* reading, std::uint8_t address)
{
    if (reading == nullptr || bus_ == nullptr || !bus_->ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    std::array<std::uint8_t, 8> request{
        address,
        kReadInputRegisters,
        static_cast<std::uint8_t>(kFirstRegister >> 8),
        static_cast<std::uint8_t>(kFirstRegister & 0xFF),
        static_cast<std::uint8_t>(kRegisterCount >> 8),
        static_cast<std::uint8_t>(kRegisterCount & 0xFF),
        0,
        0,
    };
    const auto request_crc = modbus_crc16(request.data(), 6);
    request[6] = static_cast<std::uint8_t>(request_crc & 0xFF);
    request[7] = static_cast<std::uint8_t>(request_crc >> 8);

    std::array<std::uint8_t, 32> response{};
    std::size_t response_size = 0;
    const auto error = bus_->transact(
        request.data(),
        request.size(),
        response.data(),
        response.size(),
        &response_size,
        500);
    if (error != ESP_OK) {
        return error;
    }

    if (response_size != kExpectedResponseSize ||
        response[0] != address ||
        response[1] != kReadInputRegisters ||
        response[2] != 20) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const auto received_crc = static_cast<std::uint16_t>(response[23]) |
        (static_cast<std::uint16_t>(response[24]) << 8);
    const auto calculated_crc = modbus_crc16(response.data(), 23);
    if (received_crc != calculated_crc) {
        return ESP_ERR_INVALID_CRC;
    }

    const auto voltage = be16(&response[3]);
    const auto current_low = be16(&response[5]);
    const auto current_high = be16(&response[7]);
    const auto power_low = be16(&response[9]);
    const auto power_high = be16(&response[11]);
    const auto energy_low = be16(&response[13]);
    const auto energy_high = be16(&response[15]);
    const auto frequency = be16(&response[17]);
    const auto power_factor = be16(&response[19]);
    const auto alarm = be16(&response[21]);

    reading->voltage_v = static_cast<float>(voltage) / 10.0F;
    reading->current_a = static_cast<float>(pzem32(current_low, current_high)) / 1000.0F;
    reading->power_w = static_cast<float>(pzem32(power_low, power_high)) / 10.0F;
    reading->energy_wh = pzem32(energy_low, energy_high);
    reading->frequency_hz = static_cast<float>(frequency) / 10.0F;
    reading->power_factor = static_cast<float>(power_factor) / 100.0F;
    reading->alarm = alarm != 0;
    return ESP_OK;
}

}  // namespace homeguard::idf
