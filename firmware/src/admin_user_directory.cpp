#include "homeguard/admin_user_directory.hpp"

#include <algorithm>

namespace homeguard {
namespace {

template <std::size_t N>
void copy_text(std::array<char, N>& destination, const char* source)
{
    destination.fill('\0');
    if (source == nullptr) return;
    const auto length = std::min<std::size_t>(N - 1U, std::char_traits<char>::length(source));
    std::copy_n(source, length, destination.data());
}

}

bool build_admin_user_directory(
    const AccessControl& access,
    std::string_view requester_id,
    AdminUserDirectory& out)
{
    out = AdminUserDirectory{};
    const auto* requester = access.find_user(requester_id);
    if (requester == nullptr || !requester->enabled || requester->role != AccessRole::Admin) {
        return false;
    }

    for (std::size_t index = 0; index < access.user_count(); ++index) {
        const auto* user = access.user_at(index);
        if (user == nullptr || out.count >= out.users.size()) continue;
        auto& view = out.users[out.count++];
        copy_text(view.id, user->id.data());
        copy_text(view.name, user->name.data());
        view.role = user->role;
        view.enabled = user->enabled;
    }
    return true;
}

}  // namespace homeguard
