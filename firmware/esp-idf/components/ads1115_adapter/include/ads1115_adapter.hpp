#pragma once
#include <cstdint>
class Ads1115Adapter {
public:
    bool begin(uint8_t address);
    float read_pressure(uint8_t channel);
    [[nodiscard]] bool available() const { return available_; }
private:
    bool available_{};
};
