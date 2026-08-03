#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace homeguard::idf {

class Rs485Runtime {
public:
    esp_err_t initialize(
        std::uint32_t baud_rate = 9600);

    esp_err_t transact(
        const std::uint8_t* request,
        std::size_t request_size,
        std::uint8_t* response,
        std::size_t response_capacity,
        std::size_t* response_size,
        std::uint32_t timeout_ms);

    bool ready() const noexcept;

private:
    bool initialized_{false};
};

std::uint16_t modbus_crc16(
    const std::uint8_t* data,
    std::size_t size) noexcept;

}  // namespace homeguard::idf
