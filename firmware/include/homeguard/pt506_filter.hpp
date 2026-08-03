#pragma once

#include <cstdint>

namespace homeguard {

enum class PressureLoopState {
    Ok,
    OpenLoop,
    Underrange,
    Overrange,
    ElectricalFault,
};

struct Pt506Calibration {
    float shunt_ohms{120.0F};
    float zero_current_ma{4.0F};
    float full_scale_current_ma{20.0F};
    float full_scale_bar{10.0F};
    float zero_offset_bar{0.0F};
    float gain{1.0F};
};

struct PressureReading {
    float millivolts{0.0F};
    float current_ma{0.0F};
    float pressure_bar{0.0F};
    float filtered_bar{0.0F};
    float rate_bar_per_second{0.0F};
    PressureLoopState state{PressureLoopState::ElectricalFault};
    std::uint32_t sample_count{0};
};

class Pt506Filter {
public:
    explicit Pt506Filter(
        Pt506Calibration calibration = {},
        float alpha = 0.20F);

    PressureReading update(
        float millivolts,
        std::uint64_t timestamp_ms);

    const PressureReading& reading() const noexcept;

private:
    PressureLoopState classify(float current_ma) const noexcept;
    float convert(float current_ma) const noexcept;

    Pt506Calibration calibration_;
    float alpha_;
    bool initialized_{false};
    std::uint64_t previous_timestamp_ms_{0};
    PressureReading reading_{};
};

const char* to_string(PressureLoopState state) noexcept;

}  // namespace homeguard
