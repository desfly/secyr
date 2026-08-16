#include "homeguard/hardware_capabilities.hpp"

namespace hg {

HardwareCapabilities derive_capabilities(const BoardPinMap& pins) {
    HardwareCapabilities result{};
    result.i2c = i2c_assigned(pins) ? CapabilityState::Configured : CapabilityState::Unavailable;
    result.w5500 = w5500_assigned(pins) ? CapabilityState::Configured : CapabilityState::Unavailable;
    result.service_button = pins.service_button != gpio_unassigned
        ? CapabilityState::Configured : CapabilityState::Unavailable;

    // HW-678 actuator outputs are not direct BoardPinMap GPIO capabilities.
    // They live on MCP23017 Port A and remain dangerous/unavailable at this
    // static profile layer. Runtime permission is established only by the
    // schema-v2 hardware verification + commissioning + MCP backend chain.
    result.outputs = CapabilityState::Unavailable;
    result.configured_output_count = 0;
    result.all_dangerous_outputs_disabled = true;
    return result;
}

}  // namespace hg
