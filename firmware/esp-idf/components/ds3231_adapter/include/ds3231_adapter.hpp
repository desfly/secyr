#pragma once
#include <cstdint>
class Ds3231Adapter {
public:
    bool begin();
    [[nodiscard]] uint64_t epoch() const;
    [[nodiscard]] bool available() const { return available_; }
private:
    bool available_{};
};
