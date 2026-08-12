#pragma once

#include "homeguard/access_control.hpp"
#include "homeguard/user_output_access.hpp"
#include "homeguard/user_zone_access.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace homeguard {

constexpr std::uint32_t kConfigSchemaVersion = 1;
constexpr std::size_t kConfigZoneCapacity = 16;
constexpr std::size_t kConfigOutputCapacity = 16;
constexpr std::size_t kConfigUserCapacity = AccessControl::user_capacity;

enum class ConfigZoneType : std::uint8_t {
    EntryExit,
    Perimeter,
    Interior,
    Instant,
    Fire24h,
    Flood24h,
    Tamper24h,
    Panic24h,
};

enum class ConfigOutputType : std::uint8_t { Relay, Siren, Valve, Light };

struct ConfigZone {
    std::uint16_t id{};
    std::array<char, 24> name{};
    ConfigZoneType type{ConfigZoneType::Perimeter};
    bool enabled{true};
    bool bypassed{};
    std::uint32_t entry_delay_sec{30};
    std::uint32_t exit_delay_sec{30};
};

struct ConfigOutput {
    std::uint16_t id{};
    std::array<char, 24> name{};
    ConfigOutputType type{ConfigOutputType::Relay};
    bool enabled{true};
    std::uint32_t timeout_sec{};
};

struct ConfigUser {
    std::array<char, 24> id{};
    std::array<char, 32> name{};
    AccessRole role{AccessRole::Guest};
    bool enabled{true};
};

struct HomeGuardConfigDocument {
    std::uint32_t schema_version{kConfigSchemaVersion};
    std::uint32_t default_entry_delay_sec{30};
    std::uint32_t default_exit_delay_sec{30};
    std::array<ConfigZone, kConfigZoneCapacity> zones{};
    std::size_t zone_count{};
    std::array<ConfigOutput, kConfigOutputCapacity> outputs{};
    std::size_t output_count{};
    std::array<ConfigUser, kConfigUserCapacity> users{};
    std::size_t user_count{};
    UserZoneAccess zone_access{};
    UserOutputAccess output_access{};
};

enum class ConfigValidationError : std::uint8_t {
    None,
    UnsupportedVersion,
    TooManyZones,
    TooManyOutputs,
    TooManyUsers,
    DuplicateZoneId,
    DuplicateOutputId,
    DuplicateUserId,
    InvalidZone,
    InvalidOutput,
    InvalidUser,
    MissingEnabledAdmin,
    GuestControlDenied,
};

struct ConfigValidationResult {
    ConfigValidationError error{ConfigValidationError::None};
    std::uint16_t resource_id{};
    std::array<char, 24> user_id{};
    [[nodiscard]] bool ok() const noexcept { return error == ConfigValidationError::None; }
};

enum class ConfigImportError : std::uint8_t {
    None,
    MalformedJson,
    WrongSchema,
    MissingRequiredField,
    CapacityExceeded,
    InvalidValue,
    ValidationFailed,
};

struct ConfigImportResult {
    ConfigImportError error{ConfigImportError::None};
    ConfigValidationResult validation{};
    std::size_t offset{};
    [[nodiscard]] bool ok() const noexcept { return error == ConfigImportError::None; }
};

[[nodiscard]] ConfigValidationResult validate_config_document(const HomeGuardConfigDocument& document);
[[nodiscard]] std::string export_config_json(const HomeGuardConfigDocument& document);
[[nodiscard]] ConfigImportResult import_config_json(std::string_view json, HomeGuardConfigDocument& destination);
[[nodiscard]] const char* to_string(ConfigValidationError error) noexcept;
[[nodiscard]] const char* to_string(ConfigImportError error) noexcept;

}  // namespace homeguard
