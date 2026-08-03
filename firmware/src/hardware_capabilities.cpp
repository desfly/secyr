#include "homeguard/hardware_capabilities.hpp"

#include <array>

namespace hg {

HardwareCapabilities derive_capabilities(const BoardPinMap& pins) {
    HardwareCapabilities result{};
    result.i2c = i2c_assigned(pins) ? CapabilityState::Configured : CapabilityState::Unavailable;
    result.w5500 = w5500_assigned(pins) ? CapabilityState::Configured : CapabilityState::Unavailable;
    result.service_button = pins.service_button != gpio_unassigned
        ? CapabilityState::Configured : CapabilityState::Unavailable;

    const std::array<int, 5> output_pins{pins.siren, pins.valve1, pins.valve2, pins.aux1, pins.aux2};
    for (const int pin : output_pins) {
        if (pin != gpio_unassigned) ++result.configured_output_count;
    }
    result.outputs = result.configured_output_count == 0
        ? CapabilityState::Unavailable : CapabilityState::Configured;
    result.all_dangerous_outputs_disabled = result.configured_output_count == 0;
    return result;
}

}  // namespace hg
