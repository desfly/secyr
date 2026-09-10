#pragma once

#include <array>
#include <cstdint>

namespace homeguard {

struct ZoneCalibration {
    float short_max_mv{350.0F};
    float low_alarm_min_mv{550.0F};
    float low_alarm_max_mv{1100.0F};
    float normal_min_mv{1250.0F};
    float normal_max_mv{1950.0F};
    float high_alarm_min_mv{2000.0F};
    float high_alarm_max_mv{2550.0F};
    float open_min_mv{2950.0F};
};

struct PressureCalibration {
    float shunt_ohms{120.0F};
    float current_zero_ma{4.0F};
    float current_full_scale_ma{20.0F};
    float pressure_full_scale_bar{10.0F};
    float zero_offset_bar{0.0F};
    float gain{1.0F};
};

struct HardwareCalibration {
    std::array<ZoneCalibration, 5> zones{};
    std::array<PressureCalibration, 2> pressure{};
    std::uint32_t crc32{0};
};

float pressure_from_millivolts(
    float millivolts,
    const PressureCalibration& calibration) noexcept;

bool validate_hardware_calibration(
    const HardwareCalibration& calibration) noexcept;

}  // namespace homeguard
