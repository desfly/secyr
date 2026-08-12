#include "homeguard/config_exchange.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <string>

namespace homeguard {
namespace {

class JsonCursor {
public:
    explicit JsonCursor(std::string_view input) : input_(input) {}
    [[nodiscard]] std::size_t offset() const { return pos_; }
    void ws() { while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_; }
    bool consume(char ch) { ws(); if (pos_ >= input_.size() || input_[pos_] != ch) return false; ++pos_; return true; }
    bool literal(std::string_view value) { ws(); if (input_.substr(pos_, value.size()) != value) return false; pos_ += value.size(); return true; }
    bool boolean(bool& value) { if (literal("true")) { value = true; return true; } if (literal("false")) { value = false; return true; } return false; }
    bool number(std::uint32_t& value) {
        ws();
        const char* first = input_.data() + pos_;
        const char* last = input_.data() + input_.size();
        std::uint32_t parsed{};
        const auto result = std::from_chars(first, last, parsed);
        if (result.ec != std::errc{} || result.ptr == first) return false;
        pos_ = static_cast<std::size_t>(result.ptr - input_.data());
        value = parsed;
        return true;
    }
    bool string(std::string& out) {
        ws();
        if (pos_ >= input_.size() || input_[pos_] != '"') return false;
        ++pos_;
        out.clear();
        while (pos_ < input_.size()) {
            const char ch = input_[pos_++];
            if (ch == '"') return true;
            if (static_cast<unsigned char>(ch) < 0x20U) return false;
            if (ch != '\\') { out.push_back(ch); continue; }
            if (pos_ >= input_.size()) return false;
            const char escaped = input_[pos_++];
            switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: return false; // \u intentionally rejected: exported UTF-8 is preferred.
            }
        }
        return false;
    }
    bool end() { ws(); return pos_ == input_.size(); }
    bool skip_value() {
        ws();
        if (pos_ >= input_.size()) return false;
        if (input_[pos_] == '"') { std::string temp; return string(temp); }
        if (input_[pos_] == '{') {
            if (!consume('{')) return false;
            ws(); if (consume('}')) return true;
            while (true) {
                std::string key; if (!string(key) || !consume(':') || !skip_value()) return false;
                if (consume('}')) return true;
                if (!consume(',')) return false;
            }
        }
        if (input_[pos_] == '[') {
            if (!consume('[')) return false;
            ws(); if (consume(']')) return true;
            while (true) {
                if (!skip_value()) return false;
                if (consume(']')) return true;
                if (!consume(',')) return false;
            }
        }
        bool b{};
        const auto saved = pos_;
        if (boolean(b)) return true;
        pos_ = saved;
        if (literal("null")) return true;
        pos_ = saved;
        std::uint32_t number_value{};
        return number(number_value);
    }
private:
    std::string_view input_;
    std::size_t pos_{};
};

template <std::size_t N>
bool set_text(std::array<char, N>& out, const std::string& value)
{
    if (value.empty() || value.size() >= N) return false;
    out.fill('\0');
    std::copy(value.begin(), value.end(), out.begin());
    return true;
}

bool zone_type_from(std::string_view value, ConfigZoneType& type)
{
    if (value == "entry_exit") type = ConfigZoneType::EntryExit;
    else if (value == "perimeter") type = ConfigZoneType::Perimeter;
    else if (value == "interior") type = ConfigZoneType::Interior;
    else if (value == "instant") type = ConfigZoneType::Instant;
    else if (value == "fire_24h") type = ConfigZoneType::Fire24h;
    else if (value == "flood_24h") type = ConfigZoneType::Flood24h;
    else if (value == "tamper_24h") type = ConfigZoneType::Tamper24h;
    else if (value == "panic_24h") type = ConfigZoneType::Panic24h;
    else return false;
    return true;
}

bool output_type_from(std::string_view value, ConfigOutputType& type)
{
    if (value == "relay") type = ConfigOutputType::Relay;
    else if (value == "siren") type = ConfigOutputType::Siren;
    else if (value == "valve") type = ConfigOutputType::Valve;
    else if (value == "light") type = ConfigOutputType::Light;
    else return false;
    return true;
}

bool role_from(std::string_view value, AccessRole& role)
{
    if (value == "admin") role = AccessRole::Admin;
    else if (value == "user") role = AccessRole::User;
    else if (value == "guest") role = AccessRole::Guest;
    else return false;
    return true;
}

bool parse_defaults(JsonCursor& c, HomeGuardConfigDocument& doc)
{
    if (!c.consume('{')) return false;
    bool entry = false, exit = false;
    if (c.consume('}')) return false;
    while (true) {
        std::string key; if (!c.string(key) || !c.consume(':')) return false;
        if (key == "entryDelaySec") { entry = c.number(doc.default_entry_delay_sec); if (!entry) return false; }
        else if (key == "exitDelaySec") { exit = c.number(doc.default_exit_delay_sec); if (!exit) return false; }
        else if (!c.skip_value()) return false;
        if (c.consume('}')) return entry && exit;
        if (!c.consume(',')) return false;
    }
}

bool parse_zone(JsonCursor& c, ConfigZone& zone)
{
    if (!c.consume('{')) return false;
    bool has_id=false, has_name=false, has_type=false;
    if (c.consume('}')) return false;
    while (true) {
        std::string key; if (!c.string(key) || !c.consume(':')) return false;
        if (key == "id") { std::uint32_t v{}; if (!c.number(v) || v > 65535U) return false; zone.id=static_cast<std::uint16_t>(v); has_id=true; }
        else if (key == "name") { std::string v; if (!c.string(v) || !set_text(zone.name,v)) return false; has_name=true; }
        else if (key == "type") { std::string v; if (!c.string(v) || !zone_type_from(v,zone.type)) return false; has_type=true; }
        else if (key == "enabled") { if (!c.boolean(zone.enabled)) return false; }
        else if (key == "bypassed") { if (!c.boolean(zone.bypassed)) return false; }
        else if (key == "entryDelaySec") { if (!c.number(zone.entry_delay_sec)) return false; }
        else if (key == "exitDelaySec") { if (!c.number(zone.exit_delay_sec)) return false; }
        else if (!c.skip_value()) return false;
        if (c.consume('}')) return has_id && has_name && has_type;
        if (!c.consume(',')) return false;
    }
}

bool parse_zones(JsonCursor& c, HomeGuardConfigDocument& doc)
{
    if (!c.consume('[')) return false;
    doc.zone_count=0;
    if (c.consume(']')) return true;
    while (true) {
        if (doc.zone_count >= doc.zones.size() || !parse_zone(c,doc.zones[doc.zone_count])) return false;
        ++doc.zone_count;
        if (c.consume(']')) return true;
        if (!c.consume(',')) return false;
    }
}

bool parse_output(JsonCursor& c, ConfigOutput& output)
{
    if (!c.consume('{')) return false;
    bool has_id=false, has_name=false, has_type=false;
    if (c.consume('}')) return false;
    while (true) {
        std::string key; if (!c.string(key) || !c.consume(':')) return false;
        if (key == "id") { std::uint32_t v{}; if (!c.number(v) || v>65535U) return false; output.id=static_cast<std::uint16_t>(v); has_id=true; }
        else if (key == "name") { std::string v; if (!c.string(v) || !set_text(output.name,v)) return false; has_name=true; }
        else if (key == "type") { std::string v; if (!c.string(v) || !output_type_from(v,output.type)) return false; has_type=true; }
        else if (key == "enabled") { if (!c.boolean(output.enabled)) return false; }
        else if (key == "timeoutSec") { if (!c.number(output.timeout_sec)) return false; }
        else if (!c.skip_value()) return false;
        if (c.consume('}')) return has_id && has_name && has_type;
        if (!c.consume(',')) return false;
    }
}

bool parse_outputs(JsonCursor& c, HomeGuardConfigDocument& doc)
{
    if (!c.consume('[')) return false;
    doc.output_count=0;
    if (c.consume(']')) return true;
    while (true) {
        if (doc.output_count >= doc.outputs.size() || !parse_output(c,doc.outputs[doc.output_count])) return false;
        ++doc.output_count;
        if (c.consume(']')) return true;
        if (!c.consume(',')) return false;
    }
}

bool parse_user(JsonCursor& c, ConfigUser& user)
{
    if (!c.consume('{')) return false;
    bool has_id=false, has_name=false, has_role=false;
    if (c.consume('}')) return false;
    while (true) {
        std::string key; if (!c.string(key) || !c.consume(':')) return false;
        if (key == "id") { std::string v; if (!c.string(v) || !set_text(user.id,v)) return false; has_id=true; }
        else if (key == "name") { std::string v; if (!c.string(v) || !set_text(user.name,v)) return false; has_name=true; }
        else if (key == "role") { std::string v; if (!c.string(v) || !role_from(v,user.role)) return false; has_role=true; }
        else if (key == "enabled") { if (!c.boolean(user.enabled)) return false; }
        else if (!c.skip_value()) return false;
        if (c.consume('}')) return has_id && has_name && has_role;
        if (!c.consume(',')) return false;
    }
}

bool parse_users(JsonCursor& c, HomeGuardConfigDocument& doc)
{
    if (!c.consume('[')) return false;
    doc.user_count=0;
    if (c.consume(']')) return true;
    while (true) {
        if (doc.user_count >= doc.users.size() || !parse_user(c,doc.users[doc.user_count])) return false;
        ++doc.user_count;
        if (c.consume(']')) return true;
        if (!c.consume(',')) return false;
    }
}

bool parse_zone_rule(JsonCursor& c, std::string_view user, UserZoneAccess& table)
{
    if (!c.consume('{')) return false;
    std::uint32_t id{}; bool has_id=false; ZoneAccessRule rule{};
    if (c.consume('}')) return false;
    while (true) {
        std::string key; if (!c.string(key) || !c.consume(':')) return false;
        if (key=="id") { if(!c.number(id) || id>65535U) return false; has_id=true; }
        else if(key=="view") { if(!c.boolean(rule.visible)) return false; }
        else if(key=="arm") { if(!c.boolean(rule.can_arm)) return false; }
        else if(key=="disarm") { if(!c.boolean(rule.can_disarm)) return false; }
        else if(key=="bypass") { if(!c.boolean(rule.can_bypass)) return false; }
        else if(!c.skip_value()) return false;
        if(c.consume('}')) return has_id && table.set_rule(user,static_cast<std::uint16_t>(id),rule);
        if(!c.consume(',')) return false;
    }
}

bool parse_zone_access_entry(JsonCursor& c, UserZoneAccess& table)
{
    if(!c.consume('{')) return false;
    std::string user; bool has_user=false, has_zones=false;
    if(c.consume('}')) return false;
    while(true){
        std::string key; if(!c.string(key)||!c.consume(':')) return false;
        if(key=="userId") { if(!c.string(user)||user.empty()||user.size()>=24) return false; has_user=true; }
        else if(key=="zones") {
            if(!has_user || !c.consume('[')) return false;
            if(!c.consume(']')) { while(true){ if(!parse_zone_rule(c,user,table)) return false; if(c.consume(']')) break; if(!c.consume(',')) return false; } }
            has_zones=true;
        } else if(!c.skip_value()) return false;
        if(c.consume('}')) return has_user && has_zones;
        if(!c.consume(',')) return false;
    }
}

bool parse_zone_access(JsonCursor& c, HomeGuardConfigDocument& doc)
{
    doc.zone_access={};
    if(!c.consume('[')) return false;
    if(c.consume(']')) return true;
    while(true){ if(!parse_zone_access_entry(c,doc.zone_access)) return false; if(c.consume(']')) return true; if(!c.consume(',')) return false; }
}

bool parse_output_rule(JsonCursor& c, std::string_view user, UserOutputAccess& table)
{
    if(!c.consume('{')) return false;
    std::uint32_t id{}; bool has_id=false; OutputAccessRule rule{};
    if(c.consume('}')) return false;
    while(true){
        std::string key; if(!c.string(key)||!c.consume(':')) return false;
        if(key=="id") { if(!c.number(id)||id>65535U) return false; has_id=true; }
        else if(key=="view") { if(!c.boolean(rule.visible)) return false; }
        else if(key=="on") { if(!c.boolean(rule.can_on)) return false; }
        else if(key=="off") { if(!c.boolean(rule.can_off)) return false; }
        else if(!c.skip_value()) return false;
        if(c.consume('}')) return has_id && table.set_rule(user,static_cast<std::uint16_t>(id),rule);
        if(!c.consume(',')) return false;
    }
}

bool parse_output_access_entry(JsonCursor& c, UserOutputAccess& table)
{
    if(!c.consume('{')) return false;
    std::string user; bool has_user=false, has_outputs=false;
    if(c.consume('}')) return false;
    while(true){
        std::string key; if(!c.string(key)||!c.consume(':')) return false;
        if(key=="userId") { if(!c.string(user)||user.empty()||user.size()>=24) return false; has_user=true; }
        else if(key=="outputs") {
            if(!has_user||!c.consume('[')) return false;
            if(!c.consume(']')) { while(true){ if(!parse_output_rule(c,user,table)) return false; if(c.consume(']')) break; if(!c.consume(',')) return false; } }
            has_outputs=true;
        } else if(!c.skip_value()) return false;
        if(c.consume('}')) return has_user&&has_outputs;
        if(!c.consume(',')) return false;
    }
}

bool parse_output_access(JsonCursor& c, HomeGuardConfigDocument& doc)
{
    doc.output_access={};
    if(!c.consume('[')) return false;
    if(c.consume(']')) return true;
    while(true){ if(!parse_output_access_entry(c,doc.output_access)) return false; if(c.consume(']')) return true; if(!c.consume(',')) return false; }
}

} // namespace

ConfigImportResult import_config_json(std::string_view json, HomeGuardConfigDocument& destination)
{
    ConfigImportResult result{};
    JsonCursor c(json);
    HomeGuardConfigDocument candidate{};
    bool schema=false, version=false, defaults=false, zones=false, outputs=false, users=false, zone_access=false, output_access=false;
    if(!c.consume('{')) { result.error=ConfigImportError::MalformedJson; result.offset=c.offset(); return result; }
    if(c.consume('}')) { result.error=ConfigImportError::MissingRequiredField; return result; }
    while(true){
        std::string key;
        if(!c.string(key)||!c.consume(':')) { result.error=ConfigImportError::MalformedJson; result.offset=c.offset(); return result; }
        bool ok=true;
        if(key=="schema") { std::string value; ok=c.string(value); schema=ok&&value=="homeguard-s3-config"; if(ok&&!schema){result.error=ConfigImportError::WrongSchema;result.offset=c.offset();return result;} }
        else if(key=="version") { ok=c.number(candidate.schema_version); version=ok; }
        else if(key=="defaults") { ok=parse_defaults(c,candidate); defaults=ok; }
        else if(key=="zones") { ok=parse_zones(c,candidate); zones=ok; }
        else if(key=="outputs") { ok=parse_outputs(c,candidate); outputs=ok; }
        else if(key=="users") { ok=parse_users(c,candidate); users=ok; }
        else if(key=="zoneAccess") { ok=parse_zone_access(c,candidate); zone_access=ok; }
        else if(key=="outputAccess") { ok=parse_output_access(c,candidate); output_access=ok; }
        else ok=c.skip_value();
        if(!ok){result.error=ConfigImportError::InvalidValue;result.offset=c.offset();return result;}
        if(c.consume('}')) break;
        if(!c.consume(',')){result.error=ConfigImportError::MalformedJson;result.offset=c.offset();return result;}
    }
    if(!c.end()){result.error=ConfigImportError::MalformedJson;result.offset=c.offset();return result;}
    if(!(schema&&version&&defaults&&zones&&outputs&&users&&zone_access&&output_access)){
        result.error=ConfigImportError::MissingRequiredField;result.offset=c.offset();return result;
    }
    const auto validation=validate_config_document(candidate);
    if(!validation.ok()){result.error=ConfigImportError::ValidationFailed;result.validation=validation;return result;}
    destination=candidate;
    return result;
}

const char* to_string(ConfigImportError error) noexcept
{
    switch(error){
        case ConfigImportError::MalformedJson:return "malformed_json";
        case ConfigImportError::WrongSchema:return "wrong_schema";
        case ConfigImportError::MissingRequiredField:return "missing_required_field";
        case ConfigImportError::CapacityExceeded:return "capacity_exceeded";
        case ConfigImportError::InvalidValue:return "invalid_value";
        case ConfigImportError::ValidationFailed:return "validation_failed";
        default:return "none";
    }
}

} // namespace homeguard
