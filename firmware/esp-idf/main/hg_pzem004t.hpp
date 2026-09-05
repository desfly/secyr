#pragma once

#include "driver/uart.h"
#include "esp_err.h"

#include <cstdint>

namespace homeguard::idf {

struct Pzem004tReading {
    float voltage_v{0.0F};
    float current_a{0.0F};
    float active_power_w{0.0F};
    float energy_kwh{0.0F};
    float frequency_hz{0.0F};
    float power_factor{0.0F};
    bool power_alarm{false};
};

class Pzem004t {
public:
    esp_err_t initialize(
        uart_port_t uart_port,
        int tx_gpio,
        int rx_gpio,
        std::uint8_t address = 0x01);

    esp_err_t read(Pzem004tReading* reading, std::uint32_t timeout_ms = 500);
    esp_err_t reset_energy(std::uint32_t timeout_ms = 500);

    bool ready() const noexcept;
    std::uint8_t address() const noexcept;

private:
    esp_err_t transact(
        const std::uint8_t* request,
        std::size_t request_size,
        std::uint8_t* response,
        std::size_t response_capacity,
        std::size_t* response_size,
        std::uint32_t timeout_ms);

    static std::uint16_t crc16(const std::uint8_t* data, std::size_t size) noexcept;

    uart_port_t uart_port_{UART_NUM_MAX};
    std::uint8_t address_{0x01};
    bool initialized_{false};
};

}  // namespace homeguard::idf
