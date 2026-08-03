#pragma once

#include "homeguard/hardware_profile.hpp"
#include <cstdint>
#include <string_view>

namespace hg {

enum class CapabilityState : uint8_t {
    Unavailable,
    Configured,
    RuntimeReady,
    Fault,
};

struct HardwareCapabilities {
    CapabilityState i2c{CapabilityState::Unavailable};
    CapabilityState w5500{CapabilityState::Unavailable};
    CapabilityState service_button{CapabilityState::Unavailable};
    CapabilityState outputs{CapabilityState::Unavailable};
    uint8_t configured_output_count{};
    bool all_dangerous_outputs_disabled{true};

    [[nodiscard]] constexpr bool safe_for_unverified_board() const {
        return all_dangerous_outputs_disabled && outputs == CapabilityState::Unavailable;
    }
};

[[nodiscard]] HardwareCapabilities derive_capabilities(const BoardPinMap& pins);
[[nodiscard]] constexpr std::string_view to_string(const CapabilityState state) {
    switch (state) {
        case CapabilityState::Configured: return "configured";
        case CapabilityState::RuntimeReady: return "runtime_ready";
        case CapabilityState::Fault: return "fault";
        default: return "unavailable";
    }
}

}  // namespace hg
