#include "hg_ds3231.hpp"
#include "hg_i2c_bus.hpp"
#include <cstdint>

namespace homeguard::idf {

std::uint8_t Ds3231::bcd_to_binary(
    std::uint8_t value) noexcept
{
    return static_cast<std::uint8_t>(
        (value >> 4) * 10 + (value & 0x0F));
}

std::uint8_t Ds3231::binary_to_bcd(
    std::uint8_t value) noexcept
{
    return static_cast<std::uint8_t>(
        ((value / 10) << 4) | (value % 10));
}

esp_err_t Ds3231::initialize(
    I2cBus& bus,
    std::uint8_t address)
{
    if (initialized_) {
        return ESP_OK;
    }

    initialized_ = false;
    device_ = nullptr;

    auto error = bus.add_device(
        address,
        100000,
        &device_);
    if (error != ESP_OK) {
        return error;
    }

    // Verify real register I/O before exposing the RTC as Ready. A software
    // handle alone is not evidence that the physical DS3231 is operational.
    std::uint8_t status_register = 0x0F;
    std::uint8_t status = 0;
    error = i2c_master_transmit_receive(
        device_,
        &status_register,
        1,
        &status,
        1,
        100);
    if (error != ESP_OK) {
        (void)bus.remove_device(&device_);
        return error;
    }

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Ds3231::read_time(std::tm* result)
{
    if (!initialized_ || device_ == nullptr || result == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    std::uint8_t start = 0x00;
    std::uint8_t data[7]{};

    const auto error =
        i2c_master_transmit_receive(
            device_,
            &start,
            1,
            data,
            sizeof(data),
            100);
    if (error != ESP_OK) {
        return error;
    }

    result->tm_sec = bcd_to_binary(data[0] & 0x7F);
    result->tm_min = bcd_to_binary(data[1] & 0x7F);
    result->tm_hour = bcd_to_binary(data[2] & 0x3F);
    result->tm_mday = bcd_to_binary(data[4] & 0x3F);
    result->tm_mon =
        static_cast<int>(
            bcd_to_binary(data[5] & 0x1F)) - 1;
    result->tm_year =
        static_cast<int>(bcd_to_binary(data[6])) + 100;
    result->tm_isdst = -1;

    return ESP_OK;
}

esp_err_t Ds3231::write_time(const std::tm& value)
{
    if (!initialized_ || device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    const std::uint8_t data[]{
        0x00,
        binary_to_bcd(
            static_cast<std::uint8_t>(value.tm_sec)),
        binary_to_bcd(
            static_cast<std::uint8_t>(value.tm_min)),
        binary_to_bcd(
            static_cast<std::uint8_t>(value.tm_hour)),
        1,
        binary_to_bcd(
            static_cast<std::uint8_t>(value.tm_mday)),
        binary_to_bcd(
            static_cast<std::uint8_t>(value.tm_mon + 1)),
        binary_to_bcd(
            static_cast<std::uint8_t>(
                (value.tm_year + 1900) - 2000)),
    };

    return i2c_master_transmit(
        device_,
        data,
        sizeof(data),
        100);
}

esp_err_t Ds3231::read_temperature(float* celsius)
{
    if (!initialized_ || device_ == nullptr || celsius == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    std::uint8_t start = 0x11;
    std::uint8_t data[2]{};
    const auto error =
        i2c_master_transmit_receive(
            device_,
            &start,
            1,
            data,
            sizeof(data),
            100);

    if (error == ESP_OK) {
        const auto integer =
            static_cast<std::int8_t>(data[0]);
        const float fraction =
            static_cast<float>(data[1] >> 6) * 0.25F;
        *celsius =
            static_cast<float>(integer) + fraction;
    }
    return error;
}

bool Ds3231::ready() const noexcept
{
    return initialized_ && device_ != nullptr;
}

}  // namespace homeguard::idf
