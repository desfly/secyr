#pragma once

#include <cstdint>

namespace homeguard::idf {

struct Pressure420Config {
    float shunt_ohms{250.0F};
    float min_current_ma{4.0F};
    float max_current_ma{20.0F};
    float min_bar{0.0F};
    float max_bar{10.0F};
};

struct Pressure420Reading {
    bool valid{false};
    float millivolts{0.0F};
    float current_ma{0.0F};
    float pressure_bar{0.0F};
};

Pressure420Reading decode_pressure_420ma(
    float millivolts,
    const Pressure420Config& config = {});

}  // namespace homeguard::idf
