#include "hg_config_backup.hpp"

#include "homeguard/access_store.hpp"
#include "cJSON.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace homeguard::idf {
namespace {

constexpr char kFormat[] = "homeguard-config";

std::string hex_encode(std::span<const std::byte> bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2U);
    for (const auto value : bytes) {
        const auto byte = static_cast<unsigned>(value);
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

int hex_nibble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
}

bool hex_decode(std::string_view text, std::span<std::byte> out) {
    if (text.size() != out.size() * 2U) return false;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = hex_nibble(text[i * 2U]);
        const int lo = hex_nibble(text[i * 2U + 1U]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::byte>((hi << 4) | lo);
    }
    return true;
}

const cJSON* required(const cJSON* object, const char* key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

bool read_bool(const cJSON* object, const char* key, bool& out) {
    const auto* item = required(object, key);
    if (!cJSON_IsBool(item)) return false;
    out = cJSON_IsTrue(item);
    return true;
}

bool read_string(const cJSON* object, const char* key, std::string& out, std::size_t max_len) {
    const auto* item = required(object, key);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) return false;
    const std::size_t len = std::strlen(item->valuestring);
    if (len > max_len) return false;
    out.assign(item->valuestring, len);
    return true;
}

bool read_u32(const cJSON* object, const char* key, std::uint32_t& out) {
    const auto* item = required(object, key);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0.0 || item->valuedouble > 4294967295.0) return false;
    const auto value = static_cast<std::uint64_t>(item->valuedouble);
    if (static_cast<double>(value) != item->valuedouble) return false;
    out = static_cast<std::uint32_t>(value);
    return true;
}

bool read_u64(const cJSON* object, const char* key, std::uint64_t& out) {
    const auto* item = required(object, key);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0.0) return false;
    const auto value = static_cast<std::uint64_t>(item->valuedouble);
    if (static_cast<double>(value) != item->valuedouble) return false;
    out = value;
    return true;
}

void add_commissioning(cJSON* root, const ConfigBackupV1& backup) {
    auto* object = cJSON_AddObjectToObject(root, "commissioning");
    cJSON_AddBoolToObject(object, "present", backup.commissioning_present);
    if (!backup.commissioning_present) return;
    cJSON_AddNumberToObject(object, "schemaVersion", backup.commissioning.schema_version);
    cJSON_AddBoolToObject(object, "gpioMapVerified", backup.commissioning.gpio_map_verified);
    cJSON_AddBoolToObject(object, "activePolarityVerified", backup.commissioning.active_polarity_verified);
    cJSON_AddNumberToObject(object, "successfulDryRuns", backup.commissioning.successful_dry_runs);
    cJSON_AddNumberToObject(object, "successfulActuatorTests", backup.commissioning.successful_actuator_tests);
    cJSON_AddNumberToObject(object, "lastVerifiedAtMs", static_cast<double>(backup.commissioning.last_verified_at_ms));
}

}  // namespace

std::string ConfigBackupV1Codec::encode(const ConfigBackupV1& backup) {
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) return {};

    cJSON_AddStringToObject(root, "format", kFormat);
    cJSON_AddNumberToObject(root, "version", ConfigBackupV1::version);
    cJSON_AddBoolToObject(root, "secretsIncluded", backup.secrets_included);

    const auto access_image = homeguard::AccessStoreCodec::encode(backup.access);
    const auto access_hex = hex_encode(access_image);
    cJSON_AddStringToObject(root, "accessImageHex", access_hex.c_str());

    auto* wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddBoolToObject(wifi, "present", backup.wifi_present);
    if (backup.wifi_present) {
        cJSON_AddStringToObject(wifi, "ssid", backup.wifi_ssid.c_str());
        cJSON_AddStringToObject(wifi, "password", backup.secrets_included ? backup.wifi_password.c_str() : "");
    }

    auto* cloud = cJSON_AddObjectToObject(root, "cloud");
    cJSON_AddBoolToObject(cloud, "present", backup.cloud_present);
    if (backup.cloud_present) {
        cJSON_AddBoolToObject(cloud, "enabled", backup.cloud.enabled);
        cJSON_AddStringToObject(cloud, "brokerUri", backup.cloud.broker_uri.c_str());
        cJSON_AddStringToObject(cloud, "username", backup.cloud.username.c_str());
        cJSON_AddStringToObject(cloud, "password", backup.secrets_included ? backup.cloud.password.c_str() : "");
    }

    add_commissioning(root, backup);

    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == nullptr) return {};
    std::string out{printed};
    cJSON_free(printed);
    return out;
}

bool ConfigBackupV1Codec::decode(std::string_view json, ConfigBackupV1& backup, std::string& reason) {
    backup = {};
    reason.clear();
    if (json.empty() || json.size() > 8192U) {
        reason = "invalid_size";
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(json.data(), json.size());
    if (root == nullptr || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        reason = "invalid_json";
        return false;
    }

    auto fail = [&](const char* why) {
        reason = why;
        cJSON_Delete(root);
        return false;
    };

    std::string format;
    if (!read_string(root, "format", format, 32U) || format != kFormat) return fail("invalid_format");

    const auto* version = required(root, "version");
    if (!cJSON_IsNumber(version) || version->valueint != ConfigBackupV1::version) return fail("unsupported_version");

    if (!read_bool(root, "secretsIncluded", backup.secrets_included)) return fail("missing_secrets_flag");

    std::string access_hex;
    if (!read_string(root, "accessImageHex", access_hex, homeguard::AccessStoreCodec::image_size * 2U)) return fail("invalid_access_image");
    homeguard::AccessStoreCodec::Image access_image{};
    if (!hex_decode(access_hex, access_image) || !homeguard::AccessStoreCodec::decode(access_image, backup.access)) {
        return fail("invalid_access_image");
    }

    const auto* wifi = required(root, "wifi");
    if (!cJSON_IsObject(wifi) || !read_bool(wifi, "present", backup.wifi_present)) return fail("invalid_wifi");
    if (backup.wifi_present) {
        if (!read_string(wifi, "ssid", backup.wifi_ssid, 32U) || backup.wifi_ssid.empty()) return fail("invalid_wifi_ssid");
        if (!read_string(wifi, "password", backup.wifi_password, 64U)) return fail("invalid_wifi_password");
        if (!backup.wifi_password.empty() && backup.wifi_password.size() < 8U) return fail("invalid_wifi_password");
        if (!backup.secrets_included && !backup.wifi_password.empty()) return fail("redacted_backup_has_wifi_secret");
    }

    const auto* cloud = required(root, "cloud");
    if (!cJSON_IsObject(cloud) || !read_bool(cloud, "present", backup.cloud_present)) return fail("invalid_cloud");
    if (backup.cloud_present) {
        if (!read_bool(cloud, "enabled", backup.cloud.enabled)) return fail("invalid_cloud_enabled");
        if (!read_string(cloud, "brokerUri", backup.cloud.broker_uri, 256U)) return fail("invalid_cloud_broker");
        if (!read_string(cloud, "username", backup.cloud.username, 128U)) return fail("invalid_cloud_username");
        if (!read_string(cloud, "password", backup.cloud.password, 128U)) return fail("invalid_cloud_password");
        if (backup.cloud.enabled && backup.cloud.broker_uri.empty()) return fail("invalid_cloud_broker");
        if (!backup.secrets_included && !backup.cloud.password.empty()) return fail("redacted_backup_has_cloud_secret");
    }

    const auto* commissioning = required(root, "commissioning");
    if (!cJSON_IsObject(commissioning) || !read_bool(commissioning, "present", backup.commissioning_present)) {
        return fail("invalid_commissioning");
    }
    if (backup.commissioning_present) {
        if (!read_u32(commissioning, "schemaVersion", backup.commissioning.schema_version) ||
            !read_bool(commissioning, "gpioMapVerified", backup.commissioning.gpio_map_verified) ||
            !read_bool(commissioning, "activePolarityVerified", backup.commissioning.active_polarity_verified) ||
            !read_u32(commissioning, "successfulDryRuns", backup.commissioning.successful_dry_runs) ||
            !read_u32(commissioning, "successfulActuatorTests", backup.commissioning.successful_actuator_tests) ||
            !read_u64(commissioning, "lastVerifiedAtMs", backup.commissioning.last_verified_at_ms)) {
            return fail("invalid_commissioning");
        }
        if (hg::validate_commissioning_state(backup.commissioning) != hg::CommissioningStateValidation::Valid) {
            return fail("invalid_commissioning_state");
        }
    }

    cJSON_Delete(root);
    return true;
}

}  // namespace homeguard::idf
