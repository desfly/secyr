#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <cstdint>
namespace homeguard::idf {

class I2cBus {
public:
    esp_err_t initialize();
    esp_err_t add_device(
        std::uint8_t address,
        std::uint32_t speed_hz,
        i2c_master_dev_handle_t* output);
    i2c_master_bus_handle_t handle() const noexcept;
    bool ready() const noexcept;

private:
    i2c_master_bus_handle_t bus_{nullptr};
};

}  // namespace homeguard::idf
