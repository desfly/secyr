#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hg {

enum class BleRemoteAction : std::uint8_t {
    ArmHome,
    ArmAway,
    Disarm,
    Light,
    LockPulse,
    Panic,
};

enum class BleRemoteDecision : std::uint8_t {
    Accepted,
    UnknownRemote,
    ActionDenied,
    Replay,
    InvalidEvent,
};

struct BleRemotePermissions {
    bool arm_home{};
    bool arm_away{};
    bool disarm{};
    bool light{};
    bool lock_pulse{};
    bool panic{};

    [[nodiscard]] bool allows(BleRemoteAction action) const;
};

struct BleRemoteBinding {
    std::array<std::uint8_t, 16> identity{};
    BleRemotePermissions permissions{};
    std::uint32_t last_counter{};
    bool counter_initialized{};
    bool occupied{};
};

struct BleRemoteEvent {
    std::array<std::uint8_t, 16> identity{};
    BleRemoteAction action{BleRemoteAction::Panic};
    std::uint32_t counter{};
    bool has_counter{};
};

struct BleRemoteResult {
    BleRemoteDecision decision{BleRemoteDecision::InvalidEvent};
    BleRemoteAction action{BleRemoteAction::Panic};
    std::size_t binding_index{};
};

class BleRemoteRegistry {
public:
    static constexpr std::size_t kMaxBindings = 8;

    [[nodiscard]] bool bind(const std::array<std::uint8_t, 16>& identity,
                            BleRemotePermissions permissions);
    [[nodiscard]] bool unbind(const std::array<std::uint8_t, 16>& identity);
    [[nodiscard]] BleRemoteResult authorize(const BleRemoteEvent& event);
    [[nodiscard]] std::size_t count() const;

private:
    [[nodiscard]] BleRemoteBinding* find(const std::array<std::uint8_t, 16>& identity);
    [[nodiscard]] const BleRemoteBinding* find(const std::array<std::uint8_t, 16>& identity) const;

    std::array<BleRemoteBinding, kMaxBindings> bindings_{};
};

[[nodiscard]] std::string_view ble_remote_action_name(BleRemoteAction action);
[[nodiscard]] std::string_view ble_remote_decision_name(BleRemoteDecision decision);

}  // namespace hg
