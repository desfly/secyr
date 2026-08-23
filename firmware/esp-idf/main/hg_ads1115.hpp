#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <array>
#include <cstdint>

namespace homeguard::idf {

class I2cBus;

class Ads1115 {
public:
    esp_err_t initialize(
        I2cBus& bus,
        std::uint8_t address);

    esp_err_t read_single_ended_mv(
        std::uint8_t channel,
        float* millivolts);

    esp_err_t read_all_single_ended_mv(
        std::array<float, 4>* millivolts,
        std::array<bool, 4>* valid);

    bool ready() const noexcept;
    std::uint8_t address() const noexcept;

private:
    esp_err_t write_register(
        std::uint8_t reg,
        std::uint16_t value);
    esp_err_t read_register(
        std::uint8_t reg,
        std::uint16_t* value);

    i2c_master_dev_handle_t device_{nullptr};
    SemaphoreHandle_t mutex_{nullptr};
    std::uint8_t address_{0};
};

}  // namespace homeguard::idf
