#include "hg_i2c_bus.hpp"
#include "hg_board_hw678.hpp"
#include <cstdint>

namespace homeguard::idf {

esp_err_t I2cBus::initialize()
{
    if (bus_ != nullptr) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t config{
        .i2c_port = I2C_NUM_0,
        .sda_io_num = board::kI2cSda,
        .scl_io_num = board::kI2cScl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd = false,
        },
    };

    return i2c_new_master_bus(&config, &bus_);
}

esp_err_t I2cBus::probe(
    std::uint8_t address,
    int timeout_ms) const
{
    if (bus_ == nullptr || address > 0x7FU || timeout_ms < -1) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_probe(bus_, address, timeout_ms);
}

esp_err_t I2cBus::add_device(
    std::uint8_t address,
    std::uint32_t speed_hz,
    i2c_master_dev_handle_t* output)
{
    if (bus_ == nullptr || output == nullptr || speed_hz == 0U) {
        return ESP_ERR_INVALID_STATE;
    }

    *output = nullptr;
    const auto probe_error = probe(address);
    if (probe_error != ESP_OK) {
        return probe_error;
    }

    const i2c_device_config_t config{
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = speed_hz,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = false,
        },
    };

    return i2c_master_bus_add_device(bus_, &config, output);
}

esp_err_t I2cBus::remove_device(
    i2c_master_dev_handle_t* device)
{
    if (device == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (*device == nullptr) {
        return ESP_OK;
    }

    const auto error = i2c_master_bus_rm_device(*device);
    if (error == ESP_OK) {
        *device = nullptr;
    }
    return error;
}

i2c_master_bus_handle_t I2cBus::handle() const noexcept
{
    return bus_;
}

bool I2cBus::ready() const noexcept
{
    return bus_ != nullptr;
}

}  // namespace homeguard::idf
