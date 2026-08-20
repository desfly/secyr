#include "hg_ina226.hpp"
#include "hg_i2c_bus.hpp"
#include "esp_check.h"
#include <cstdint>

namespace homeguard::idf {

namespace {

constexpr std::uint8_t kConfig = 0x00;
constexpr std::uint8_t kShuntVoltage = 0x01;
constexpr std::uint8_t kBusVoltage = 0x02;
constexpr std::uint8_t kPower = 0x03;
constexpr std::uint8_t kCurrent = 0x04;
constexpr std::uint8_t kCalibration = 0x05;

}  // namespace

esp_err_t Ina226::initialize(
    I2cBus& bus,
    std::uint8_t address,
    float shunt_ohms,
    float maximum_current_a)
{
    if (initialized_) {
        return ESP_OK;
    }
    if (shunt_ohms <= 0.0F || maximum_current_a <= 0.0F) {
        return ESP_ERR_INVALID_ARG;
    }

    initialized_ = false;
    auto error = bus.add_device(
        address,
        400000,
        &device_);
    if (error != ESP_OK) {
        return error;
    }

    current_lsb_a_ = maximum_current_a / 32768.0F;
    power_lsb_w_ = current_lsb_a_ * 25.0F;

    const auto calibration =
        static_cast<std::uint16_t>(
            0.00512F /
            (current_lsb_a_ * shunt_ohms));

    if ((error = write_register(kConfig, 0x4527)) != ESP_OK) {
        return error;
    }
    if ((error = write_register(kCalibration, calibration)) != ESP_OK) {
        return error;
    }

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Ina226::write_register(
    std::uint8_t reg,
    std::uint16_t value)
{
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

esp_err_t Ina226::read_register(
    std::uint8_t reg,
    std::uint16_t* value)
{
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

esp_err_t Ina226::read(Ina226Reading* reading)
{
    if (reading == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized_ || device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    std::uint16_t bus_raw = 0;
    std::uint16_t shunt_raw = 0;
    std::uint16_t current_raw = 0;
    std::uint16_t power_raw = 0;

    ESP_RETURN_ON_ERROR(
        read_register(kBusVoltage, &bus_raw),
        "ina226",
        "bus voltage");
    ESP_RETURN_ON_ERROR(
        read_register(kShuntVoltage, &shunt_raw),
        "ina226",
        "shunt voltage");
    ESP_RETURN_ON_ERROR(
        read_register(kCurrent, &current_raw),
        "ina226",
        "current");
    ESP_RETURN_ON_ERROR(
        read_register(kPower, &power_raw),
        "ina226",
        "power");

    reading->bus_voltage_v =
        static_cast<float>(bus_raw) * 0.00125F;
    reading->shunt_voltage_mv =
        static_cast<float>(
            static_cast<std::int16_t>(shunt_raw)) *
        0.0025F;
    reading->current_a =
        static_cast<float>(
            static_cast<std::int16_t>(current_raw)) *
        current_lsb_a_;
    reading->power_w =
        static_cast<float>(power_raw) *
        power_lsb_w_;

    return ESP_OK;
}

bool Ina226::ready() const noexcept
{
    return initialized_;
}

}  // namespace homeguard::idf
