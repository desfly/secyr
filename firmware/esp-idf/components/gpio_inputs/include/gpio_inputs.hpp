#pragma once
#include <cstdint>
class GpioInputs {
public:
    bool begin();
    [[nodiscard]] bool loop_closed(uint8_t zone) const;
    [[nodiscard]] bool tamper(uint8_t zone) const;
    [[nodiscard]] bool available() const { return available_; }
private:
    bool available_{};
};
