#pragma once
#include "homeguard/maintenance.hpp"
class SafeOutputs {
public:
    bool begin();
    void apply(const hg::Outputs& outputs);
    void all_off();
    [[nodiscard]] bool available() const { return available_; }
    [[nodiscard]] hg::Outputs last_requested() const { return last_requested_; }
private:
    bool available_{};
    hg::Outputs last_requested_{};
};
