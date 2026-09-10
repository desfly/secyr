#pragma once

#include "homeguard/system_model.hpp"

#include <cstdint>
#include <string_view>

namespace hg {

enum class SecurityCommandStatus : std::uint8_t {
    Applied,
    Unsupported,
    Failed,
};

struct SecurityCommandResult {
    SecurityCommandStatus status{SecurityCommandStatus::Unsupported};
    PartitionArmState resulting_state{PartitionArmState::Disarmed};
};

[[nodiscard]] SecurityCommandResult apply_security_command(
    SystemModel& model,
    SystemEventBus& bus,
    std::string_view command,
    std::uint64_t now_ms = 0);

[[nodiscard]] const char* to_string(SecurityCommandStatus status) noexcept;

}  // namespace hg
