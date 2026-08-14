#pragma once

#include "esp_err.h"

#include <cstdint>

namespace homeguard::idf {

class Rs485Runtime;

struct Pzem004tReading {
    float voltage_v{0.0F};
    float current_a{0.0F};
    float power_w{0.0F};
    std::uint32_t energy_wh{0};
    float frequency_hz{0.0F};
    float power_factor{0.0F};
    bool alarm{false};
};

class Pzem004t {
public:
    explicit Pzem004t(Rs485Runtime* bus = nullptr) noexcept;

    void attach(Rs485Runtime* bus) noexcept;
    esp_err_t read(Pzem004tReading* reading, std::uint8_t address = 0xF8);

private:
    Rs485Runtime* bus_{nullptr};
};

}  // namespace homeguard::idf
