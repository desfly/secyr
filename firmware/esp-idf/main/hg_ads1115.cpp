#include "hg_ads1115.hpp"
#include "hg_i2c_bus.hpp"
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace homeguard::idf {

namespace {

constexpr std::uint8_t kConversionRegister = 0x00;
constexpr std::uint8_t kConfigRegister = 0x01;
constexpr std::uint16_t kStart = 0x8000;
constexpr std::uint16_t kSingleShot = 0x0100;
constexpr std::uint16_t kPga4096 = 0x0200;
constexpr std::uint16_t kDataRate128 = 0x0080;
constexpr std::uint16_t kComparatorDisabled = 0x0003;

}  // namespace

esp_err_t Ads1115::initialize(
    I2cBus& bus,
    std::uint8_t address)
{
    if (address < 0x48 || address > 0x4B) {
        return ESP_ERR_INVALID_ARG;
    }
    if (initialized_) {
        return address_ == address ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    initialized_ = false;
    address_ = 0;
    device_ = nullptr;

    auto error = bus.add_device(
        address,
        400000,
        &device_);
    if (error != ESP_OK) {
        return error;
    }

    // A successful handle allocation is not sufficient to declare the ADC
    // operational. Read a real register so initialization is transactional.
    std::uint16_t config = 0;
    error = read_register(kConfigRegister, &config);
    if (error != ESP_OK) {
        (void)bus.remove_device(&device_);
        return error;
    }

    address_ = address;
    initialized_ = true;
    return ESP_OK;
}

esp_err_t Ads1115::write_register(
    std::uint8_t reg,
    std::uint16_t value)
{
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    const std::uint8_t data[]{
        reg,
        static_cast<std::uint8_t>(value >> 8),
        static_cast<std::uint8_t>(value & 0xFF),
    };
    return i2c_master_transmit(
        device_,
        data,
        sizeof(data),
        100);
}

esp_err_t Ads1115::read_register(
    std::uint8_t reg,
    std::uint16_t* value)
{
    if (device_ == nullptr || value == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    std::uint8_t bytes[2]{};
    const auto error = i2c_master_transmit_receive(
        device_,
        &reg,
        1,
        bytes,
        sizeof(bytes),
        100);

    if (error == ESP_OK) {
        *value =
            static_cast<std::uint16_t>(bytes[0] << 8) |
            bytes[1];
    }
    return error;
}

esp_err_t Ads1115::read_single_ended_mv(
    std::uint8_t channel,
    float* millivolts)
{
    if (!initialized_ || device_ == nullptr || millivolts == nullptr || channel > 3) {
        return ESP_ERR_INVALID_STATE;
    }

    const std::uint16_t mux =
        static_cast<std::uint16_t>(0x4000 + (channel << 12));
    const std::uint16_t config =
        kStart | mux | kPga4096 | kSingleShot |
        kDataRate128 | kComparatorDisabled;

    auto error = write_register(kConfigRegister, config);
    if (error != ESP_OK) {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    std::uint16_t raw_unsigned = 0;
    error = read_register(kConversionRegister, &raw_unsigned);
    if (error != ESP_OK) {
        return error;
    }

    const auto raw = static_cast<std::int16_t>(raw_unsigned);
    *millivolts = static_cast<float>(raw) * 0.125F;
    return ESP_OK;
}

bool Ads1115::ready() const noexcept
{
    return initialized_ && device_ != nullptr;
}

std::uint8_t Ads1115::address() const noexcept
{
    return address_;
}

}  // namespace homeguard::idf
