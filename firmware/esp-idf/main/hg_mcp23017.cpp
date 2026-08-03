#include "hg_mcp23017.hpp"
#include "hg_i2c_bus.hpp"
#include <cstdint>

namespace homeguard::idf {

namespace {

constexpr std::uint8_t kIodirA = 0x00;
constexpr std::uint8_t kIodirB = 0x01;
constexpr std::uint8_t kGppuB = 0x0D;
constexpr std::uint8_t kGpioB = 0x13;
constexpr std::uint8_t kOlatA = 0x14;

}  // namespace

esp_err_t Mcp23017::initialize(
    I2cBus& bus,
    std::uint8_t address)
{
    auto error = bus.add_device(
        address,
        400000,
        &device_);
    if (error != ESP_OK) {
        return error;
    }

    if ((error = write_register(kIodirA, 0x00)) != ESP_OK) {
        return error;
    }
    if ((error = write_register(kIodirB, 0xFF)) != ESP_OK) {
        return error;
    }
    if ((error = write_register(kGppuB, 0xFF)) != ESP_OK) {
        return error;
    }

    return force_safe_outputs();
}

esp_err_t Mcp23017::write_register(
    std::uint8_t reg,
    std::uint8_t value)
{
    const std::uint8_t data[]{reg, value};
    return i2c_master_transmit(
        device_,
        data,
        sizeof(data),
        100);
}

esp_err_t Mcp23017::read_register(
    std::uint8_t reg,
    std::uint8_t* value)
{
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_transmit_receive(
        device_,
        &reg,
        1,
        value,
        1,
        100);
}

esp_err_t Mcp23017::force_safe_outputs()
{
    return write_register(kOlatA, 0x00);
}

esp_err_t Mcp23017::write_outputs(std::uint8_t value)
{
    return write_register(kOlatA, value);
}

esp_err_t Mcp23017::read_inputs(std::uint8_t* value)
{
    return read_register(kGpioB, value);
}

bool Mcp23017::ready() const noexcept
{
    return device_ != nullptr;
}

}  // namespace homeguard::idf
