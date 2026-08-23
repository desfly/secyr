#include "hg_ads1115.hpp"
#include "hg_i2c_bus.hpp"

#include <array>
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
constexpr TickType_t kAccessTimeout = pdMS_TO_TICKS(250);

}  // namespace

esp_err_t Ads1115::initialize(
    I2cBus& bus,
    std::uint8_t address)
{
    if (address < 0x48 || address > 0x4B) {
        return ESP_ERR_INVALID_ARG;
    }

    device_ = nullptr;
    mutex_ = nullptr;
    address_ = 0;

    i2c_master_dev_handle_t candidate = nullptr;
    auto error = bus.add_device(
        address,
        400000,
        &candidate);
    if (error != ESP_OK) {
        return error;
    }

    // Adding a device handle does not prove that hardware exists. Bind the
    // candidate temporarily and perform a real register transaction so READY
    // means the ADS1115 actually acknowledged on the physical I2C bus.
    device_ = candidate;
    std::uint16_t config = 0;
    error = read_register(kConfigRegister, &config);
    if (error != ESP_OK) {
        (void)i2c_master_bus_rm_device(candidate);
        device_ = nullptr;
        address_ = 0;
        return error;
    }

    // A conversion is a multi-step transaction: select MUX, wait, then read.
    // Protect the whole sequence because telemetry and diagnostic HTTP may read
    // the same ADC concurrently.
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        (void)i2c_master_bus_rm_device(candidate);
        device_ = nullptr;
        address_ = 0;
        return ESP_ERR_NO_MEM;
    }

    address_ = address;
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
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
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
    if (device_ == nullptr || mutex_ == nullptr || millivolts == nullptr || channel > 3) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(mutex_, kAccessTimeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    const std::uint16_t mux =
        static_cast<std::uint16_t>(0x4000 + (channel << 12));
    const std::uint16_t config =
        kStart | mux | kPga4096 | kSingleShot |
        kDataRate128 | kComparatorDisabled;

    auto error = write_register(kConfigRegister, config);
    if (error == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));

        std::uint16_t raw_unsigned = 0;
        error = read_register(kConversionRegister, &raw_unsigned);
        if (error == ESP_OK) {
            const auto raw = static_cast<std::int16_t>(raw_unsigned);
            *millivolts = static_cast<float>(raw) * 0.125F;
        }
    }

    (void)xSemaphoreGive(mutex_);
    return error;
}

esp_err_t Ads1115::read_all_single_ended_mv(
    std::array<float, 4>* millivolts,
    std::array<bool, 4>* valid)
{
    if (device_ == nullptr || millivolts == nullptr || valid == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    millivolts->fill(0.0F);
    valid->fill(false);
    esp_err_t first_error = ESP_OK;

    for (std::uint8_t channel = 0; channel < 4; ++channel) {
        float value = 0.0F;
        const auto error = read_single_ended_mv(channel, &value);
        if (error == ESP_OK) {
            (*millivolts)[channel] = value;
            (*valid)[channel] = true;
        } else if (first_error == ESP_OK) {
            first_error = error;
        }
    }

    return first_error;
}

bool Ads1115::ready() const noexcept
{
    return device_ != nullptr && mutex_ != nullptr;
}

std::uint8_t Ads1115::address() const noexcept
{
    return address_;
}

}  // namespace homeguard::idf
