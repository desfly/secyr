#include "homeguard/hardware_calibration.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace homeguard;

int main()
{
    HardwareCalibration calibration;
    assert(validate_hardware_calibration(calibration));

    const auto& pressure = calibration.pressure[0];

    assert(
        std::fabs(
            pressure_from_millivolts(480.0F, pressure) -
            0.0F) < 0.001F);

    assert(
        std::fabs(
            pressure_from_millivolts(1440.0F, pressure) -
            5.0F) < 0.001F);

    assert(
        std::fabs(
            pressure_from_millivolts(2400.0F, pressure) -
            10.0F) < 0.001F);

    calibration.zones[0].normal_min_mv = 100.0F;
    assert(!validate_hardware_calibration(calibration));

    std::cout << "Build-0027 hardware calibration tests PASS\n";
    return 0;
}
