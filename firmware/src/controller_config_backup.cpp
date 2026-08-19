#include "homeguard/controller_config_backup.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>

namespace hg {
namespace {

bool locate_value(std::string_view json, std::string_view key, std::string_view& value) {
    const std::string marker = std::string{"\""} + std::string{key} + "\"";
    const auto key_pos = json.find(marker);
    if (key_pos == std::string_view::npos) return false;
    const auto colon = json.find(':', key_pos + marker.size());
    if (colon == std::string_view::npos) return false;
    auto start = json.find_first_not_of(" \t\r\n", colon + 1U);
    if (start == std::string_view::npos) return false;
    if (json[start] == '"') {
        const auto end = json.find('"', start + 1U);
        if (end == std::string_view::npos) return false;
        value = json.substr(start + 1U, end - start - 1U);
        return true;
    }
    const auto end = json.find_first_of(",}\r\n", start);
    value = json.substr(start, end == std::string_view::npos ? json.size() - start : end - start);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1U);
    return !value.empty();
}

bool read_string(std::string_view json, std::string_view key, std::string& out) {
    std::string_view value;
    if (!locate_value(json, key, value)) return false;
    out.assign(value.begin(), value.end());
    return true;
}

bool read_bool(std::string_view json, std::string_view key, bool& out) {
    std::string_view value;
    if (!locate_value(json, key, value)) return false;
    if (value == "true") { out = true; return true; }
    if (value == "false") { out = false; return true; }
    return false;
}

bool read_u32(std::string_view json, std::string_view key, std::uint32_t& out) {
    std::string_view value;
    if (!locate_value(json, key, value)) return false;
    std::uint32_t parsed{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) return false;
    out = parsed;
    return true;
}

bool read_float(std::string_view json, std::string_view key, float& out) {
    std::string_view value;
    if (!locate_value(json, key, value) || value.size() > 40U) return false;
    std::string buffer{value};
    char* end = nullptr;
    const float parsed = std::strtof(buffer.c_str(), &end);
    if (end != buffer.c_str() + buffer.size() || !std::isfinite(parsed)) return false;
    out = parsed;
    return true;
}

bool validate(const ControllerConfig& config, std::string& error) {
    if (config.entry_delay_ms > 600000U || config.exit_delay_ms > 600000U) {
        error = "entry_exit_delay_out_of_range"; return false;
    }
    if (config.network_debounce_ms > 60000U || config.failover_hold_ms > 600000U) {
        error = "network_timing_out_of_range"; return false;
    }
    if (config.request_ttl_ms < 1000U || config.request_ttl_ms > 3600000U) {
        error = "request_ttl_out_of_range"; return false;
    }
    for (const auto& zone : config.zones) {
        if (zone.debounce_ms > 10000U) { error = "zone_debounce_out_of_range"; return false; }
    }
    for (const auto& pressure : config.pressures) {
        if (!std::isfinite(pressure.low) || !std::isfinite(pressure.high) || !std::isfinite(pressure.hysteresis) ||
            pressure.low < -1000.0F || pressure.high > 1000.0F || pressure.low >= pressure.high ||
            pressure.hysteresis < 0.0F || pressure.hysteresis > 100.0F) {
            error = "pressure_range_invalid"; return false;
        }
    }
    return true;
}

std::string zone_key(std::size_t index, const char* suffix) {
    return "zone" + std::to_string(index) + suffix;
}
std::string pressure_key(std::size_t index, const char* suffix) {
    return "pressure" + std::to_string(index) + suffix;
}

}  // namespace

std::string ControllerConfigBackup::encode(const ControllerConfig& config) {
    std::ostringstream out;
    out << "{\"format\":\"" << format << "\",\"version\":" << version
        << ",\"entryDelayMs\":" << config.entry_delay_ms
        << ",\"exitDelayMs\":" << config.exit_delay_ms
        << ",\"networkDebounceMs\":" << config.network_debounce_ms
        << ",\"failoverHoldMs\":" << config.failover_hold_ms
        << ",\"requestTtlMs\":" << config.request_ttl_ms;
    for (std::size_t i = 0; i < config.zones.size(); ++i) {
        const auto& zone = config.zones[i];
        out << ",\"zone" << i << "Enabled\":" << (zone.enabled ? "true" : "false")
            << ",\"zone" << i << "NormallyClosed\":" << (zone.normally_closed ? "true" : "false")
            << ",\"zone" << i << "DebounceMs\":" << zone.debounce_ms;
    }
    for (std::size_t i = 0; i < config.pressures.size(); ++i) {
        const auto& pressure = config.pressures[i];
        out << ",\"pressure" << i << "Enabled\":" << (pressure.enabled ? "true" : "false")
            << ",\"pressure" << i << "Low\":" << pressure.low
            << ",\"pressure" << i << "High\":" << pressure.high
            << ",\"pressure" << i << "Hysteresis\":" << pressure.hysteresis;
    }
    out << "}";
    return out.str();
}

bool ControllerConfigBackup::decode(std::string_view json, ControllerConfig& config, std::string& error) {
    std::string found_format;
    std::uint32_t found_version{};
    if (!read_string(json, "format", found_format) || found_format != format) {
        error = "unsupported_format"; return false;
    }
    if (!read_u32(json, "version", found_version) || found_version != static_cast<std::uint32_t>(version)) {
        error = "unsupported_version"; return false;
    }

    ControllerConfig candidate{};
    if (!read_u32(json, "entryDelayMs", candidate.entry_delay_ms) ||
        !read_u32(json, "exitDelayMs", candidate.exit_delay_ms) ||
        !read_u32(json, "networkDebounceMs", candidate.network_debounce_ms) ||
        !read_u32(json, "failoverHoldMs", candidate.failover_hold_ms) ||
        !read_u32(json, "requestTtlMs", candidate.request_ttl_ms)) {
        error = "missing_timing_field"; return false;
    }
    for (std::size_t i = 0; i < candidate.zones.size(); ++i) {
        if (!read_bool(json, zone_key(i, "Enabled"), candidate.zones[i].enabled) ||
            !read_bool(json, zone_key(i, "NormallyClosed"), candidate.zones[i].normally_closed) ||
            !read_u32(json, zone_key(i, "DebounceMs"), candidate.zones[i].debounce_ms)) {
            error = "missing_zone_field"; return false;
        }
    }
    for (std::size_t i = 0; i < candidate.pressures.size(); ++i) {
        if (!read_bool(json, pressure_key(i, "Enabled"), candidate.pressures[i].enabled) ||
            !read_float(json, pressure_key(i, "Low"), candidate.pressures[i].low) ||
            !read_float(json, pressure_key(i, "High"), candidate.pressures[i].high) ||
            !read_float(json, pressure_key(i, "Hysteresis"), candidate.pressures[i].hysteresis)) {
            error = "missing_pressure_field"; return false;
        }
    }
    if (!validate(candidate, error)) return false;
    config = candidate;
    error.clear();
    return true;
}

}  // namespace hg
