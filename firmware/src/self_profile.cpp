#include "homeguard/self_profile.hpp"

#include <algorithm>
#include <cstring>

namespace homeguard {
namespace {

template <std::size_t N>
void copy_text(std::array<char, N>& destination, const char* source)
{
    destination.fill('\0');
    if (source == nullptr || N == 0U) return;
    const auto length = std::min<std::size_t>(std::strlen(source), N - 1U);
    std::copy_n(source, length, destination.data());
}

}  // namespace

bool build_self_profile(const AccessControl& access, std::string_view actor, SelfProfile& out)
{
    const auto* user = access.find_user(actor);
    if (user == nullptr || !user->enabled) return false;

    out = SelfProfile{};
    copy_text(out.id, user->id.data());
    copy_text(out.name, user->name.data());
    out.role = user->role;
    out.enabled = user->enabled;
    out.can_arm = access.role_allows(user->role, "security.arm_home");
    out.can_disarm = access.role_allows(user->role, "security.disarm");
    out.can_control_valves = access.role_allows(user->role, "valve.open") &&
                             access.role_allows(user->role, "valve.close");
    out.can_manage_users = access.role_allows(user->role, "access.users.manage");
    return true;
}

}  // namespace homeguard
