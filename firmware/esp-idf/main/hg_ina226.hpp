#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <cstdint>

namespace homeguard::idf {

class I2cBus;

struct Ina226Reading {
    float bus_voltage_v{0.0F};
    float shunt_voltage_mv{0.0F};
    float current_a{0.0F};
    float power_w{0.0F};
};

class Ina226 {
public:
    esp_err_t initialize(
        I2cBus& bus,
        std::uint8_t address,
        float shunt_ohms,
        float maximum_current_a);

    esp_err_t read(Ina226Reading* reading);
    bool ready() const noexcept;

private:
    esp_err_t write_register(
        std::uint8_t reg,
        std::uint16_t value);
    esp_err_t read_register(
        std::uint8_t reg,
        std::uint16_t* value);

    i2c_master_dev_handle_t device_{nullptr};
    float current_lsb_a_{0.0F};
    float power_lsb_w_{0.0F};
};

}  // namespace homeguard::idf
