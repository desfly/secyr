#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hg {

enum class BleRemoteProfile : std::uint8_t {
    HomeGuardNative,
    HidGatt,
    VerifiedVendor,
};

enum class BleRemoteAction : std::uint8_t {
    ArmAway,
    ArmHome,
    Disarm,
    LockPulse,
    LightToggle,
    Panic,
};

enum class BleRemoteDecision : std::uint8_t {
    Accepted,
    UnknownRemote,
    Disabled,
    PermissionDenied,
    ReplayRejected,
    InvalidAction,
};

struct BleRemoteIdentity {
    std::uint8_t address_type{};
    std::array<std::uint8_t, 6> address{};

    friend bool operator==(const BleRemoteIdentity&, const BleRemoteIdentity&) = default;
};

struct BleRemoteBinding {
    BleRemoteIdentity identity{};
    BleRemoteProfile profile{BleRemoteProfile::HomeGuardNative};
    std::array<char, 24> name{};
    std::uint32_t permissions{};
    bool enabled{true};
    bool replay_counter_required{true};
    bool counter_initialized{};
    std::uint32_t last_counter{};
};

struct BleRemoteEvent {
    BleRemoteIdentity identity{};
    BleRemoteAction action{BleRemoteAction::ArmAway};
    bool counter_valid{};
    std::uint32_t counter{};
};

struct BleRemoteResult {
    BleRemoteDecision decision{BleRemoteDecision::UnknownRemote};
    const BleRemoteBinding* binding{};
    const char* command{};
};

class BleRemoteRegistry {
public:
    static constexpr std::size_t capacity = 8;

    bool bind(
        const BleRemoteIdentity& identity,
        BleRemoteProfile profile,
        std::string_view name,
        std::uint32_t permissions,
        bool replay_counter_required = true);
    bool unbind(const BleRemoteIdentity& identity);
    bool set_enabled(const BleRemoteIdentity& identity, bool enabled);
    [[nodiscard]] BleRemoteBinding* find(const BleRemoteIdentity& identity);
    [[nodiscard]] const BleRemoteBinding* find(const BleRemoteIdentity& identity) const;
    [[nodiscard]] std::size_t size() const { return size_; }

    BleRemoteResult accept(const BleRemoteEvent& event);

    static constexpr std::uint32_t permission(BleRemoteAction action)
    {
        return 1U << static_cast<std::uint8_t>(action);
    }
    static const char* command_for(BleRemoteAction action);

private:
    static void copy_name(std::array<char, 24>& out, std::string_view value);

    std::array<BleRemoteBinding, capacity> bindings_{};
    std::size_t size_{};
};

[[nodiscard]] const char* to_string(BleRemoteDecision decision) noexcept;

}  // namespace hg
