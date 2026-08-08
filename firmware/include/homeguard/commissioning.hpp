#pragma once

#include "homeguard/hardware_test.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace hg {

enum class CommissioningState {
    Blocked,
    DryRunReady,
    ActuatorTestReady,
};

struct CommissioningInput {
    bool controller_alive{};
    bool zones_available{};
    bool analog_available{};
    bool event_log_available{};
    bool physical_outputs_available{};
    bool gpio_map_verified{};
    bool active_polarity_verified{};
    bool maintenance_active{};
    bool system_armed{};
    bool alarm_active{};
};

struct CommissioningCheck {
    std::string name;
    bool passed{};
    std::string detail;
};

struct CommissioningReport {
    CommissioningState state{CommissioningState::Blocked};
    std::vector<CommissioningCheck> checks;
    std::uint32_t passed{};
    std::uint32_t failed{};

    [[nodiscard]] bool dry_run_ready() const noexcept {
        return state == CommissioningState::DryRunReady || state == CommissioningState::ActuatorTestReady;
    }
    [[nodiscard]] bool actuator_test_ready() const noexcept {
        return state == CommissioningState::ActuatorTestReady;
    }
};

class CommissioningEvaluator {
public:
    [[nodiscard]] CommissioningReport evaluate(const CommissioningInput& input) const;
};

const char* to_string(CommissioningState state) noexcept;
std::string commissioning_json(const CommissioningReport& report);

}  // namespace hg
