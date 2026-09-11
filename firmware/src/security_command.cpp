#include "homeguard/security_command.hpp"

#include <cstdint>
#include <string_view>

namespace hg {

SecurityCommandResult apply_security_command(
    SystemModel& model,
    SystemEventBus& bus,
    std::string_view command,
    std::uint64_t now_ms)
{
    PartitionArmState target{};
    if (command == "security.arm_away") target = PartitionArmState::Away;
    else if (command == "security.arm_home") target = PartitionArmState::Stay;
    else if (command == "security.disarm") target = PartitionArmState::Disarmed;
    else if (command == "security.panic") target = PartitionArmState::Alarm;
    else return {SecurityCommandStatus::Unsupported, PartitionArmState::Disarmed};

    if (!model.set_partition_arm(1, target, now_ms)) {
        return {SecurityCommandStatus::Failed, target};
    }
    (void)bus.dispatch_all();
    return {SecurityCommandStatus::Applied, target};
}

const char* to_string(SecurityCommandStatus status) noexcept
{
    switch (status) {
        case SecurityCommandStatus::Applied: return "applied";
        case SecurityCommandStatus::Unsupported: return "unsupported_command";
        case SecurityCommandStatus::Failed: return "partition_command_failed";
    }
    return "unsupported_command";
}

}  // namespace hg
