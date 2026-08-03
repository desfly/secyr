#pragma once

#include "esp_err.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {

struct Ds18b20Device {
    std::array<std::uint8_t, 8> rom{};
    float temperature_c{0.0F};
    bool valid{false};
};

class OneWireRuntime {
public:
    esp_err_t initialize();
    esp_err_t discover();
    esp_err_t convert_all();
    esp_err_t read_all();

    std::size_t device_count() const noexcept;
    const Ds18b20Device* devices() const noexcept;
    bool ready() const noexcept;

private:
    static std::uint8_t crc8(
        const std::uint8_t* data,
        std::size_t size) noexcept;

    bool reset();
    void write_bit(bool value);
    bool read_bit();
    void write_byte(std::uint8_t value);
    std::uint8_t read_byte();
    bool search_next(std::array<std::uint8_t, 8>* rom);

    std::array<Ds18b20Device, 8> devices_{};
    std::size_t count_{0};
    int last_discrepancy_{0};
    bool last_device_{false};
    std::array<std::uint8_t, 8> search_rom_{};
    bool initialized_{false};
};

}  // namespace homeguard::idf
