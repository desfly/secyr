#include "homeguard/ble_remote.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace hg {

bool BleRemotePermissions::allows(BleRemoteAction action) const
{
    switch (action) {
        case BleRemoteAction::ArmHome: return arm_home;
        case BleRemoteAction::ArmAway: return arm_away;
        case BleRemoteAction::Disarm: return disarm;
        case BleRemoteAction::Light: return light;
        case BleRemoteAction::LockPulse: return lock_pulse;
        case BleRemoteAction::Panic: return panic;
    }
    return false;
}

BleRemoteBinding* BleRemoteRegistry::find(const std::array<std::uint8_t, 16>& identity)
{
    for (auto& binding : bindings_) {
        if (binding.occupied && binding.identity == identity) return &binding;
    }
    return nullptr;
}

const BleRemoteBinding* BleRemoteRegistry::find(const std::array<std::uint8_t, 16>& identity) const
{
    for (const auto& binding : bindings_) {
        if (binding.occupied && binding.identity == identity) return &binding;
    }
    return nullptr;
}

bool BleRemoteRegistry::bind(const std::array<std::uint8_t, 16>& identity,
                             BleRemotePermissions permissions)
{
    const bool nonzero = std::any_of(identity.begin(), identity.end(), [](std::uint8_t value) { return value != 0; });
    if (!nonzero) return false;

    if (auto* existing = find(identity); existing != nullptr) {
        existing->permissions = permissions;
        return true;
    }

    for (auto& binding : bindings_) {
        if (!binding.occupied) {
            binding.identity = identity;
            binding.permissions = permissions;
            binding.last_counter = 0;
            binding.counter_initialized = false;
            binding.occupied = true;
            return true;
        }
    }
    return false;
}

bool BleRemoteRegistry::unbind(const std::array<std::uint8_t, 16>& identity)
{
    auto* binding = find(identity);
    if (binding == nullptr) return false;
    *binding = {};
    return true;
}

BleRemoteResult BleRemoteRegistry::authorize(const BleRemoteEvent& event)
{
    const bool nonzero = std::any_of(event.identity.begin(), event.identity.end(), [](std::uint8_t value) { return value != 0; });
    if (!nonzero) return {BleRemoteDecision::InvalidEvent, event.action, 0};

    auto* binding = find(event.identity);
    if (binding == nullptr) return {BleRemoteDecision::UnknownRemote, event.action, 0};
    const auto index = static_cast<std::size_t>(binding - bindings_.data());

    if (!binding->permissions.allows(event.action)) {
        return {BleRemoteDecision::ActionDenied, event.action, index};
    }

    if (event.has_counter) {
        if (binding->counter_initialized && event.counter <= binding->last_counter) {
            return {BleRemoteDecision::Replay, event.action, index};
        }
        binding->last_counter = event.counter;
        binding->counter_initialized = true;
    }

    return {BleRemoteDecision::Accepted, event.action, index};
}

std::size_t BleRemoteRegistry::count() const
{
    return static_cast<std::size_t>(std::count_if(bindings_.begin(), bindings_.end(), [](const BleRemoteBinding& binding) {
        return binding.occupied;
    }));
}

std::string_view ble_remote_action_name(BleRemoteAction action)
{
    switch (action) {
        case BleRemoteAction::ArmHome: return "arm_home";
        case BleRemoteAction::ArmAway: return "arm_away";
        case BleRemoteAction::Disarm: return "disarm";
        case BleRemoteAction::Light: return "light";
        case BleRemoteAction::LockPulse: return "lock_pulse";
        case BleRemoteAction::Panic: return "panic";
    }
    return "unknown";
}

std::string_view ble_remote_decision_name(BleRemoteDecision decision)
{
    switch (decision) {
        case BleRemoteDecision::Accepted: return "accepted";
        case BleRemoteDecision::UnknownRemote: return "unknown_remote";
        case BleRemoteDecision::ActionDenied: return "action_denied";
        case BleRemoteDecision::Replay: return "replay";
        case BleRemoteDecision::InvalidEvent: return "invalid_event";
    }
    return "invalid_event";
}

}  // namespace hg
