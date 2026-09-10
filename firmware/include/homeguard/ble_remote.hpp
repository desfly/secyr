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
    std::array<char, 24> owner_user_id{};
    std::array<char, 32> name{};
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
                            std::string_view owner_user_id,
                            std::string_view name,
                            BleRemotePermissions permissions);
    [[nodiscard]] bool unbind(const std::array<std::uint8_t, 16>& identity);
    [[nodiscard]] BleRemoteResult authorize(const BleRemoteEvent& event);
    [[nodiscard]] std::size_t count() const;
    [[nodiscard]] const BleRemoteBinding* binding_at(std::size_t index) const;
    [[nodiscard]] const BleRemoteBinding* binding_for(const std::array<std::uint8_t, 16>& identity) const;
    [[nodiscard]] bool import_binding(const BleRemoteBinding& binding);
    void clear();

private:
    [[nodiscard]] BleRemoteBinding* find(const std::array<std::uint8_t, 16>& identity);
    [[nodiscard]] const BleRemoteBinding* find(const std::array<std::uint8_t, 16>& identity) const;
    static bool valid_text(std::string_view value, std::size_t capacity);
    static void copy_text(char* destination, std::size_t capacity, std::string_view source);

    std::array<BleRemoteBinding, kMaxBindings> bindings_{};
};

[[nodiscard]] std::string_view ble_remote_action_name(BleRemoteAction action);
[[nodiscard]] std::string_view ble_remote_decision_name(BleRemoteDecision decision);

}  // namespace hg
