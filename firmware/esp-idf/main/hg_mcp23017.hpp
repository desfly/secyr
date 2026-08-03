#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <cstdint>

namespace homeguard::idf {

class I2cBus;

class Mcp23017 {
public:
    esp_err_t initialize(
        I2cBus& bus,
        std::uint8_t address = 0x20);

    esp_err_t force_safe_outputs();
    esp_err_t write_outputs(std::uint8_t value);
    esp_err_t read_inputs(std::uint8_t* value);
    bool ready() const noexcept;

private:
    esp_err_t write_register(
        std::uint8_t reg,
        std::uint8_t value);
    esp_err_t read_register(
        std::uint8_t reg,
        std::uint8_t* value);

    i2c_master_dev_handle_t device_{nullptr};
};

}  // namespace homeguard::idf
