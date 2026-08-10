#pragma once

#include "homeguard/access_control.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace homeguard {

struct SelfProfile {
    std::array<char, 24> id{};
    std::array<char, 32> name{};
    AccessRole role{AccessRole::Guest};
    bool enabled{false};
    bool can_arm{false};
    bool can_disarm{false};
    bool can_control_valves{false};
    bool can_manage_users{false};
};

[[nodiscard]] bool build_self_profile(
    const AccessControl& access,
    std::string_view actor,
    SelfProfile& out);

}  // namespace homeguard
