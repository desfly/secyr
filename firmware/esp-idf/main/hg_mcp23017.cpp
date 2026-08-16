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
    if (initialized_) {
        return ESP_OK;
    }

    initialized_ = false;
    auto error = bus.add_device(
        address,
        400000,
        &device_);
    if (error != ESP_OK) {
        return error;
    }

    if ((error = write_register(kIodirA, 0x00)) == ESP_OK) {
        error = write_register(kIodirB, 0xFF);
    }
    if (error == ESP_OK) {
        error = write_register(kGppuB, 0xFF);
    }
    if (error == ESP_OK) {
        error = write_register(kOlatA, 0x00);
    }

    if (error != ESP_OK) {
        (void)bus.remove_device(&device_);
        return error;
    }

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Mcp23017::write_register(
    std::uint8_t reg,
    std::uint8_t value)
{
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
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
    if (device_ == nullptr || value == nullptr) {
        return ESP_ERR_INVALID_STATE;
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
    if (!ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    return write_register(kOlatA, 0x00);
}

esp_err_t Mcp23017::write_outputs(std::uint8_t value)
{
    if (!ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    return write_register(kOlatA, value);
}

esp_err_t Mcp23017::read_inputs(std::uint8_t* value)
{
    if (!ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    return read_register(kGpioB, value);
}

bool Mcp23017::ready() const noexcept
{
    return initialized_ && device_ != nullptr;
}

}  // namespace homeguard::idf
