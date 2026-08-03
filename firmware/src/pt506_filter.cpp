#include "homeguard/pt506_filter.hpp"

#include <algorithm>

namespace homeguard {

Pt506Filter::Pt506Filter(
    Pt506Calibration calibration,
    float alpha)
    : calibration_(calibration),
      alpha_(alpha > 0.0F && alpha <= 1.0F ? alpha : 0.20F)
{
}

PressureLoopState Pt506Filter::classify(float current_ma) const noexcept
{
    if (current_ma < 3.6F) {
        return PressureLoopState::OpenLoop;
    }
    if (current_ma < calibration_.zero_current_ma) {
        return PressureLoopState::Underrange;
    }
    if (current_ma <= calibration_.full_scale_current_ma) {
        return PressureLoopState::Ok;
    }
    if (current_ma <= 21.0F) {
        return PressureLoopState::Overrange;
    }
    return PressureLoopState::ElectricalFault;
}

float Pt506Filter::convert(float current_ma) const noexcept
{
    const float span =
        calibration_.full_scale_current_ma -
        calibration_.zero_current_ma;

    if (span <= 0.0F) {
        return 0.0F;
    }

    float normalized =
        (current_ma - calibration_.zero_current_ma) / span;
    normalized = std::clamp(normalized, 0.0F, 1.0F);

    return (
        normalized * calibration_.full_scale_bar *
        calibration_.gain) + calibration_.zero_offset_bar;
}

PressureReading Pt506Filter::update(
    float millivolts,
    std::uint64_t timestamp_ms)
{
    reading_.millivolts = millivolts;
    reading_.current_ma =
        calibration_.shunt_ohms > 0.0F ?
        millivolts / calibration_.shunt_ohms : 0.0F;
    reading_.state = classify(reading_.current_ma);
    reading_.pressure_bar = convert(reading_.current_ma);

    if (!initialized_) {
        initialized_ = true;
        reading_.filtered_bar = reading_.pressure_bar;
        reading_.rate_bar_per_second = 0.0F;
    } else {
        const float previous = reading_.filtered_bar;
        reading_.filtered_bar =
            previous * (1.0F - alpha_) +
            reading_.pressure_bar * alpha_;

        if (timestamp_ms > previous_timestamp_ms_) {
            const float seconds =
                static_cast<float>(
                    timestamp_ms - previous_timestamp_ms_) / 1000.0F;
            reading_.rate_bar_per_second =
                (reading_.filtered_bar - previous) / seconds;
        }
    }

    previous_timestamp_ms_ = timestamp_ms;
    ++reading_.sample_count;
    return reading_;
}

const PressureReading& Pt506Filter::reading() const noexcept
{
    return reading_;
}

const char* to_string(PressureLoopState state) noexcept
{
    switch (state) {
    case PressureLoopState::Ok:
        return "ok";
    case PressureLoopState::OpenLoop:
        return "open_loop";
    case PressureLoopState::Underrange:
        return "underrange";
    case PressureLoopState::Overrange:
        return "overrange";
    case PressureLoopState::ElectricalFault:
    default:
        return "electrical_fault";
    }
}

}  // namespace homeguard
