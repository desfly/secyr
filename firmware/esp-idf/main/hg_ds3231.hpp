#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <cstdint>
#include <ctime>

namespace homeguard::idf {

class I2cBus;

class Ds3231 {
public:
    esp_err_t initialize(
        I2cBus& bus,
        std::uint8_t address = 0x68);

    esp_err_t read_time(std::tm* result);
    esp_err_t write_time(const std::tm& value);
    esp_err_t read_temperature(float* celsius);
    bool ready() const noexcept;

private:
    static std::uint8_t bcd_to_binary(
        std::uint8_t value) noexcept;
    static std::uint8_t binary_to_bcd(
        std::uint8_t value) noexcept;

    i2c_master_dev_handle_t device_{nullptr};
    bool initialized_{};
};

}  // namespace homeguard::idf
