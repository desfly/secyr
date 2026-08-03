#include "homeguard/hardware_calibration.hpp"

#include <algorithm>
#include <cmath>

namespace homeguard {

float pressure_from_millivolts(
    float millivolts,
    const PressureCalibration& calibration) noexcept
{
    if (calibration.shunt_ohms <= 0.0F ||
        calibration.current_full_scale_ma <=
            calibration.current_zero_ma ||
        calibration.pressure_full_scale_bar <= 0.0F) {
        return 0.0F;
    }

    const float current_ma =
        millivolts / calibration.shunt_ohms;

    const float normalized = std::clamp(
        (current_ma - calibration.current_zero_ma) /
        (calibration.current_full_scale_ma -
         calibration.current_zero_ma),
        0.0F,
        1.0F);

    return normalized *
        calibration.pressure_full_scale_bar *
        calibration.gain +
        calibration.zero_offset_bar;
}

bool validate_hardware_calibration(
    const HardwareCalibration& calibration) noexcept
{
    for (const auto& zone : calibration.zones) {
        if (!(zone.short_max_mv <
              zone.low_alarm_min_mv &&
              zone.low_alarm_min_mv <
              zone.low_alarm_max_mv &&
              zone.low_alarm_max_mv <
              zone.normal_min_mv &&
              zone.normal_min_mv <
              zone.normal_max_mv &&
              zone.normal_max_mv <
              zone.high_alarm_min_mv &&
              zone.high_alarm_min_mv <
              zone.high_alarm_max_mv &&
              zone.high_alarm_max_mv <
              zone.open_min_mv &&
              zone.open_min_mv <= 3300.0F)) {
            return false;
        }
    }

    for (const auto& pressure : calibration.pressure) {
        if (!(pressure.shunt_ohms > 0.0F &&
              pressure.current_zero_ma >= 3.5F &&
              pressure.current_full_scale_ma > 20.0F - 0.1F &&
              pressure.pressure_full_scale_bar > 0.0F &&
              std::isfinite(pressure.gain) &&
              pressure.gain > 0.0F)) {
            return false;
        }
    }

    return true;
}

}  // namespace homeguard
