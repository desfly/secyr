#pragma once
#include <cstddef>
#include "homeguard/types.hpp"
#include <array>
#include <cstdint>
#include <string_view>
namespace hg {
enum class Component : uint8_t { Esp, Nvs, Rtc, Adc1, Adc2, W5500, Wifi, Inputs, Outputs, Count };
struct HealthEntry { HealthState state{HealthState::Unknown}; uint64_t changed_at_ms{}; uint32_t consecutive_failures{}; };
class HealthMonitor {
public:
 void report(Component component, bool success, uint64_t now_ms, uint32_t degrade_after=1, uint32_t fail_after=3);
 void set(Component component, HealthState state, uint64_t now_ms);
 [[nodiscard]] HealthEntry get(Component component) const;
 [[nodiscard]] HealthState overall() const;
 [[nodiscard]] uint32_t failed_count() const;
 [[nodiscard]] uint32_t degraded_count() const;
private: std::array<HealthEntry,static_cast<size_t>(Component::Count)> entries_{};
};
std::string_view component_name(Component c);
}
