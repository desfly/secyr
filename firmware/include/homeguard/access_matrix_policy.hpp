#pragma once

#include "homeguard/access_control.hpp"
#include "homeguard/system_model.hpp"
#include "homeguard/user_output_access.hpp"
#include "homeguard/user_zone_access.hpp"

namespace homeguard {

bool apply_default_access_for_user(const AccessUser& user,
                                   const hg::SystemModel& model,
                                   UserZoneAccess& zone_access,
                                   UserOutputAccess& output_access);

bool sync_default_access_matrices(const AccessControl& access,
                                  const hg::SystemModel& model,
                                  UserZoneAccess& zone_access,
                                  UserOutputAccess& output_access);

}  // namespace homeguard
