#include "homeguard/access_matrix_policy.hpp"

#include <cstddef>
#include <string_view>

namespace homeguard {

bool apply_default_access_for_user(const AccessUser& user,
                                   const hg::SystemModel& model,
                                   UserZoneAccess& zone_access,
                                   UserOutputAccess& output_access)
{
    const std::string_view user_id{user.id.data()};
    if (user_id.empty()) return false;
    if (!user.enabled) {
        (void)zone_access.remove_user(user_id);
        (void)output_access.remove_user(user_id);
        return true;
    }
    if (!zone_access.ensure_user(user_id) || !output_access.ensure_user(user_id)) return false;

    for (std::size_t i = 0; i < model.zone_count(); ++i) {
        const auto* zone = model.zone_at(i);
        if (zone == nullptr) continue;
        ZoneAccessRule rule{};
        rule.visible = true;
        if (user.role == AccessRole::Admin) {
            rule.can_arm = true;
            rule.can_disarm = true;
            rule.can_bypass = true;
        } else if (user.role == AccessRole::User) {
            rule.can_arm = true;
            rule.can_disarm = true;
        }
        if (!zone_access.set_rule(user_id, zone->id, rule)) return false;
    }

    for (std::size_t i = 0; i < model.output_count(); ++i) {
        const auto* output = model.output_at(i);
        if (output == nullptr) continue;
        OutputAccessRule rule{};
        rule.visible = true;
        if (user.role == AccessRole::Admin) {
            rule.can_on = true;
            rule.can_off = true;
        } else if (user.role == AccessRole::User && output->type == hg::ModelOutputType::Valve) {
            rule.can_on = true;
            rule.can_off = true;
        }
        if (!output_access.set_rule(user_id, output->id, rule)) return false;
    }
    return true;
}

bool sync_default_access_matrices(const AccessControl& access,
                                  const hg::SystemModel& model,
                                  UserZoneAccess& zone_access,
                                  UserOutputAccess& output_access)
{
    bool ok = true;
    for (std::size_t i = 0; i < access.user_count(); ++i) {
        const auto* user = access.user_at(i);
        if (user == nullptr) {
            ok = false;
            continue;
        }
        if (!apply_default_access_for_user(*user, model, zone_access, output_access)) ok = false;
    }
    return ok;
}

}  // namespace homeguard
