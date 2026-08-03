#pragma once
#include <cstdint>
namespace hg {
enum class PowerState : uint8_t { Mains, Battery, LowBattery, Critical };
class PowerMonitor {
public:
 PowerState update(bool mains_present, float battery_v);
 [[nodiscard]] PowerState state() const { return state_; }
private: PowerState state_{PowerState::Mains};
};
}
