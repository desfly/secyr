#include "homeguard/admin_payload_canonical.hpp"

namespace homeguard {
namespace {

void append_field(std::string& out, std::string_view value)
{
    out += std::to_string(value.size());
    out.push_back(':');
    out.append(value);
    out.push_back('|');
}

}  // namespace

std::string canonical_admin_target_payload(std::string_view target_id)
{
    std::string out;
    out.reserve(target_id.size() + 16U);
    append_field(out, target_id);
    return out;
}

std::string canonical_admin_upsert_payload(
    std::string_view target_id,
    std::string_view name,
    std::string_view role,
    std::string_view enabled,
    std::string_view encrypted_pin)
{
    std::string out;
    out.reserve(target_id.size() + name.size() + role.size() + enabled.size() + encrypted_pin.size() + 48U);
    append_field(out, target_id);
    append_field(out, name);
    append_field(out, role);
    append_field(out, enabled);
    append_field(out, encrypted_pin);
    return out;
}

}  // namespace homeguard
