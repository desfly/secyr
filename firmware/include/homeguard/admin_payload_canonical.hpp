#pragma once

#include <string>
#include <string_view>

namespace homeguard {

[[nodiscard]] std::string canonical_admin_target_payload(std::string_view target_id);

[[nodiscard]] std::string canonical_admin_upsert_payload(
    std::string_view target_id,
    std::string_view name,
    std::string_view role,
    std::string_view enabled,
    std::string_view encrypted_pin);

}  // namespace homeguard
