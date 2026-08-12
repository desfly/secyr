#include "homeguard/config_exchange.hpp"

#include <algorithm>
#include <sstream>

namespace homeguard {
namespace {

template <std::size_t N>
std::string_view text_view(const std::array<char, N>& value)
{
    std::size_t length = 0;
    while (length < value.size() && value[length] != '\0') ++length;
    return {value.data(), length};
}

std::string json_escape(std::string_view value)
{
    std::string out;
    out.reserve(value.size() + 8U);
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch >= 0x20U) out.push_back(static_cast<char>(ch));
                break;
        }
    }
    return out;
}

const char* role_name(AccessRole role)
{
    switch (role) {
        case AccessRole::Admin: return "admin";
        case AccessRole::User: return "user";
        default: return "guest";
    }
}

const char* zone_type_name(ConfigZoneType type)
{
    switch (type) {
        case ConfigZoneType::EntryExit: return "entry_exit";
        case ConfigZoneType::Interior: return "interior";
        case ConfigZoneType::Instant: return "instant";
        case ConfigZoneType::Fire24h: return "fire_24h";
        case ConfigZoneType::Flood24h: return "flood_24h";
        case ConfigZoneType::Tamper24h: return "tamper_24h";
        case ConfigZoneType::Panic24h: return "panic_24h";
        default: return "perimeter";
    }
}

const char* output_type_name(ConfigOutputType type)
{
    switch (type) {
        case ConfigOutputType::Siren: return "siren";
        case ConfigOutputType::Valve: return "valve";
        case ConfigOutputType::Light: return "light";
        default: return "relay";
    }
}

void copy_user_id(std::array<char, 24>& destination, std::string_view source)
{
    destination.fill('\0');
    const auto length = std::min(source.size(), destination.size() - 1U);
    std::copy_n(source.begin(), length, destination.begin());
}

bool duplicate_zone_id(const HomeGuardConfigDocument& doc, std::size_t index)
{
    for (std::size_t i = 0; i < index; ++i) if (doc.zones[i].id == doc.zones[index].id) return true;
    return false;
}

bool duplicate_output_id(const HomeGuardConfigDocument& doc, std::size_t index)
{
    for (std::size_t i = 0; i < index; ++i) if (doc.outputs[i].id == doc.outputs[index].id) return true;
    return false;
}

bool duplicate_user_id(const HomeGuardConfigDocument& doc, std::size_t index)
{
    const auto id = text_view(doc.users[index].id);
    for (std::size_t i = 0; i < index; ++i) if (text_view(doc.users[i].id) == id) return true;
    return false;
}

}  // namespace

ConfigValidationResult validate_config_document(const HomeGuardConfigDocument& doc)
{
    ConfigValidationResult result{};
    if (doc.schema_version != kConfigSchemaVersion) {
        result.error = ConfigValidationError::UnsupportedVersion;
        return result;
    }
    if (doc.zone_count > kConfigZoneCapacity) { result.error = ConfigValidationError::TooManyZones; return result; }
    if (doc.output_count > kConfigOutputCapacity) { result.error = ConfigValidationError::TooManyOutputs; return result; }
    if (doc.user_count > kConfigUserCapacity) { result.error = ConfigValidationError::TooManyUsers; return result; }
    if (doc.default_entry_delay_sec > 3600U || doc.default_exit_delay_sec > 3600U) {
        result.error = ConfigValidationError::InvalidZone;
        return result;
    }

    for (std::size_t i = 0; i < doc.zone_count; ++i) {
        const auto& zone = doc.zones[i];
        if (zone.id == 0 || zone.id > kConfigZoneCapacity || text_view(zone.name).empty() ||
            zone.entry_delay_sec > 3600U || zone.exit_delay_sec > 3600U) {
            result.error = ConfigValidationError::InvalidZone;
            result.resource_id = zone.id;
            return result;
        }
        if (duplicate_zone_id(doc, i)) {
            result.error = ConfigValidationError::DuplicateZoneId;
            result.resource_id = zone.id;
            return result;
        }
    }

    for (std::size_t i = 0; i < doc.output_count; ++i) {
        const auto& output = doc.outputs[i];
        if (output.id == 0 || output.id > kConfigOutputCapacity || text_view(output.name).empty() || output.timeout_sec > 86400U) {
            result.error = ConfigValidationError::InvalidOutput;
            result.resource_id = output.id;
            return result;
        }
        if (duplicate_output_id(doc, i)) {
            result.error = ConfigValidationError::DuplicateOutputId;
            result.resource_id = output.id;
            return result;
        }
    }

    std::size_t enabled_admins = 0;
    for (std::size_t i = 0; i < doc.user_count; ++i) {
        const auto& user = doc.users[i];
        const auto id = text_view(user.id);
        if (id.empty() || id.size() >= user.id.size() || text_view(user.name).empty()) {
            result.error = ConfigValidationError::InvalidUser;
            copy_user_id(result.user_id, id);
            return result;
        }
        if (duplicate_user_id(doc, i)) {
            result.error = ConfigValidationError::DuplicateUserId;
            copy_user_id(result.user_id, id);
            return result;
        }
        if (user.enabled && user.role == AccessRole::Admin) ++enabled_admins;

        if (user.role == AccessRole::Guest) {
            const auto* zone_record = [&]() -> const UserZoneAccessRecord* {
                for (std::size_t j = 0; j < doc.zone_access.user_count(); ++j) {
                    const auto* record = doc.zone_access.user_at(j);
                    if (record != nullptr && text_view(record->user_id) == id) return record;
                }
                return nullptr;
            }();
            if (zone_record != nullptr) {
                for (const auto& rule : zone_record->zones) {
                    if (rule.can_arm || rule.can_disarm || rule.can_bypass) {
                        result.error = ConfigValidationError::GuestControlDenied;
                        copy_user_id(result.user_id, id);
                        return result;
                    }
                }
            }
            const auto* output_record = [&]() -> const UserOutputAccessRecord* {
                for (std::size_t j = 0; j < doc.output_access.user_count(); ++j) {
                    const auto* record = doc.output_access.user_at(j);
                    if (record != nullptr && text_view(record->user_id) == id) return record;
                }
                return nullptr;
            }();
            if (output_record != nullptr) {
                for (const auto& rule : output_record->outputs) {
                    if (rule.can_on || rule.can_off) {
                        result.error = ConfigValidationError::GuestControlDenied;
                        copy_user_id(result.user_id, id);
                        return result;
                    }
                }
            }
        }
    }
    if (enabled_admins == 0) {
        result.error = ConfigValidationError::MissingEnabledAdmin;
        return result;
    }
    return result;
}

std::string export_config_json(const HomeGuardConfigDocument& doc)
{
    std::ostringstream out;
    out << "{\n  \"schema\":\"homeguard-s3-config\",\n  \"version\":" << doc.schema_version
        << ",\n  \"defaults\":{\"entryDelaySec\":" << doc.default_entry_delay_sec
        << ",\"exitDelaySec\":" << doc.default_exit_delay_sec << "},\n  \"zones\":[";
    for (std::size_t i = 0; i < doc.zone_count; ++i) {
        const auto& z = doc.zones[i];
        if (i != 0) out << ',';
        out << "\n    {\"id\":" << z.id << ",\"name\":\"" << json_escape(text_view(z.name))
            << "\",\"type\":\"" << zone_type_name(z.type) << "\",\"enabled\":" << (z.enabled ? "true" : "false")
            << ",\"bypassed\":" << (z.bypassed ? "true" : "false")
            << ",\"entryDelaySec\":" << z.entry_delay_sec << ",\"exitDelaySec\":" << z.exit_delay_sec << '}';
    }
    out << "\n  ],\n  \"outputs\":[";
    for (std::size_t i = 0; i < doc.output_count; ++i) {
        const auto& o = doc.outputs[i];
        if (i != 0) out << ',';
        out << "\n    {\"id\":" << o.id << ",\"name\":\"" << json_escape(text_view(o.name))
            << "\",\"type\":\"" << output_type_name(o.type) << "\",\"enabled\":" << (o.enabled ? "true" : "false")
            << ",\"timeoutSec\":" << o.timeout_sec << '}';
    }
    out << "\n  ],\n  \"users\":[";
    for (std::size_t i = 0; i < doc.user_count; ++i) {
        const auto& u = doc.users[i];
        if (i != 0) out << ',';
        out << "\n    {\"id\":\"" << json_escape(text_view(u.id)) << "\",\"name\":\"" << json_escape(text_view(u.name))
            << "\",\"role\":\"" << role_name(u.role) << "\",\"enabled\":" << (u.enabled ? "true" : "false") << '}';
    }
    out << "\n  ],\n  \"zoneAccess\":[";
    for (std::size_t i = 0; i < doc.zone_access.user_count(); ++i) {
        const auto* r = doc.zone_access.user_at(i);
        if (r == nullptr) continue;
        if (i != 0) out << ',';
        out << "\n    {\"userId\":\"" << json_escape(text_view(r->user_id)) << "\",\"zones\":[";
        for (std::size_t z = 0; z < r->zones.size(); ++z) {
            if (z != 0) out << ',';
            const auto& rule = r->zones[z];
            out << "{\"id\":" << (z + 1U) << ",\"view\":" << (rule.visible ? "true" : "false")
                << ",\"arm\":" << (rule.can_arm ? "true" : "false") << ",\"disarm\":" << (rule.can_disarm ? "true" : "false")
                << ",\"bypass\":" << (rule.can_bypass ? "true" : "false") << '}';
        }
        out << "]}";
    }
    out << "\n  ],\n  \"outputAccess\":[";
    for (std::size_t i = 0; i < doc.output_access.user_count(); ++i) {
        const auto* r = doc.output_access.user_at(i);
        if (r == nullptr) continue;
        if (i != 0) out << ',';
        out << "\n    {\"userId\":\"" << json_escape(text_view(r->user_id)) << "\",\"outputs\":[";
        for (std::size_t o = 0; o < r->outputs.size(); ++o) {
            if (o != 0) out << ',';
            const auto& rule = r->outputs[o];
            out << "{\"id\":" << (o + 1U) << ",\"view\":" << (rule.visible ? "true" : "false")
                << ",\"on\":" << (rule.can_on ? "true" : "false") << ",\"off\":" << (rule.can_off ? "true" : "false") << '}';
        }
        out << "]}";
    }
    out << "\n  ]\n}\n";
    return out.str();
}

const char* to_string(ConfigValidationError error) noexcept
{
    switch (error) {
        case ConfigValidationError::UnsupportedVersion: return "unsupported_version";
        case ConfigValidationError::TooManyZones: return "too_many_zones";
        case ConfigValidationError::TooManyOutputs: return "too_many_outputs";
        case ConfigValidationError::TooManyUsers: return "too_many_users";
        case ConfigValidationError::DuplicateZoneId: return "duplicate_zone_id";
        case ConfigValidationError::DuplicateOutputId: return "duplicate_output_id";
        case ConfigValidationError::DuplicateUserId: return "duplicate_user_id";
        case ConfigValidationError::InvalidZone: return "invalid_zone";
        case ConfigValidationError::InvalidOutput: return "invalid_output";
        case ConfigValidationError::InvalidUser: return "invalid_user";
        case ConfigValidationError::MissingEnabledAdmin: return "missing_enabled_admin";
        case ConfigValidationError::GuestControlDenied: return "guest_control_denied";
        default: return "none";
    }
}

}  // namespace homeguard
