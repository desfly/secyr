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
    // A failed register setup may leave a valid I2C device handle behind.
    // Reuse that handle on retry; registering the same address again would
    // leak a handle and can make recovery impossible.
    if (initialized_) return ESP_ERR_INVALID_STATE;
    if (!bus.ready()) return ESP_ERR_INVALID_STATE;
    esp_err_t error = ESP_OK;
    if (device_ == nullptr) {
        error = bus.add_device(address, 400000, &device_);
        if (error != ESP_OK) {
            device_ = nullptr;
            return error;
        }
    }

    // Preload safe OFF levels before switching port A pins to outputs.
    if ((error = force_safe_outputs()) != ESP_OK) {
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

    error = force_safe_outputs();
    initialized_ = error == ESP_OK;
    return error;
}

esp_err_t Mcp23017::write_register(
    std::uint8_t reg,
    std::uint8_t value)
{
    if (device_ == nullptr) return ESP_ERR_INVALID_STATE;
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
    if (device_ == nullptr) return ESP_ERR_INVALID_STATE;
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
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    return write_register(kOlatA, value);
}

esp_err_t Mcp23017::write_mirrored_outputs(std::uint8_t value)
{
    if (!ready()) return ESP_ERR_INVALID_STATE;
    std::uint8_t previous = 0;
    const auto error = read_register(kOlatA, &previous);
    if (error != ESP_OK) return error;
    // Only GPA0/GPA1 belong to these duplicates; preserve GPA2–GPA7.
    return write_register(kOlatA, (previous & 0xFCU) | (value & 0x03U));
}

esp_err_t Mcp23017::read_inputs(std::uint8_t* value)
{
    return read_register(kGpioB, value);
}

bool Mcp23017::ready() const noexcept
{
    return initialized_;
}

}  // namespace homeguard::idf
